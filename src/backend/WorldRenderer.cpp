#include "WorldRenderer.h"
#include "NsfHifiganRenderer.h"

#include <world/cheaptrick.h>
#include <world/d4c.h>
#include <world/harvest.h>
#include <world/synthesis.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace hachi::backend
{
namespace
{
double curveAt(const std::vector<float>& curve, double position, double fallback)
{
    if (curve.empty() || !std::isfinite(position) || position < 0.0) return fallback;
    const auto left = static_cast<std::size_t>(std::floor(position));
    if (left >= curve.size()) return fallback;
    const auto right = std::min(left + 1, curve.size() - 1);
    const auto amount = position - static_cast<double>(left);
    const auto a = static_cast<double>(curve[left]);
    const auto b = static_cast<double>(curve[right]);
    return std::isfinite(a) && std::isfinite(b) ? a + (b - a) * amount : fallback;
}

double sourceTimeAt(const std::vector<WorldTimeMapPoint>& map,
                    double targetSeconds, double sourceDuration,
                    double targetDuration)
{
    if (map.size() < 2)
        return targetDuration > 0.0
            ? std::clamp(targetSeconds / targetDuration * sourceDuration, 0.0, sourceDuration)
            : 0.0;
    const auto right = std::upper_bound(map.begin(), map.end(), targetSeconds,
        [](double time, const WorldTimeMapPoint& point) { return time < point.targetSeconds; });
    if (right == map.begin()) return std::clamp(right->sourceSeconds, 0.0, sourceDuration);
    if (right == map.end()) return std::clamp(map.back().sourceSeconds, 0.0, sourceDuration);
    const auto& a = *(right - 1);
    const auto& b = *right;
    const auto amount = b.targetSeconds > a.targetSeconds
        ? std::clamp((targetSeconds - a.targetSeconds)
                     / (b.targetSeconds - a.targetSeconds), 0.0, 1.0) : 0.0;
    return std::clamp(a.sourceSeconds + (b.sourceSeconds - a.sourceSeconds) * amount,
                      0.0, sourceDuration);
}

double matrixAt(const std::vector<std::vector<double>>& matrix,
                double framePosition, double binPosition, bool logarithmic)
{
    if (matrix.empty() || matrix.front().empty()) return logarithmic ? 1.0e-12 : 1.0;
    framePosition = std::clamp(framePosition, 0.0, static_cast<double>(matrix.size() - 1));
    binPosition = std::clamp(binPosition, 0.0, static_cast<double>(matrix.front().size() - 1));
    const auto f0 = static_cast<std::size_t>(std::floor(framePosition));
    const auto f1 = std::min(f0 + 1, matrix.size() - 1);
    const auto b0 = static_cast<std::size_t>(std::floor(binPosition));
    const auto b1 = std::min(b0 + 1, matrix.front().size() - 1);
    const auto ft = framePosition - static_cast<double>(f0);
    const auto bt = binPosition - static_cast<double>(b0);
    const auto transform = [logarithmic](double value)
    {
        return logarithmic ? std::log(std::max(1.0e-12, value)) : value;
    };
    const auto a = transform(matrix[f0][b0])
        + (transform(matrix[f0][b1]) - transform(matrix[f0][b0])) * bt;
    const auto b = transform(matrix[f1][b0])
        + (transform(matrix[f1][b1]) - transform(matrix[f1][b0])) * bt;
    const auto value = a + (b - a) * ft;
    return logarithmic ? std::exp(value) : value;
}

bool hasPitchDifference(const std::vector<float>& sourceMidi,
                        const std::vector<float>& targetMidi)
{
    const auto count = std::min(sourceMidi.size(), targetMidi.size());
    for (std::size_t index = 0; index < count; ++index)
        if (sourceMidi[index] > 0.0f && targetMidi[index] > 0.0f
            && std::abs(sourceMidi[index] - targetMidi[index]) > 1.0e-4f)
            return true;
    return false;
}
}

juce::AudioBuffer<float> WorldRenderer::renderAssembly(
    const std::vector<NsfHifiganSourcePart>& assembly, int targetSamples,
    double sampleRate, double framePeriodMs, const std::vector<float>& targetMidi,
    const std::vector<float>& formantSemitones, juce::AudioBuffer<float>* noiseOutput)
{
    if (noiseOutput != nullptr) noiseOutput->setSize(0, 0);
    if (assembly.empty() || targetSamples <= 0 || sampleRate <= 0.0) return {};
    const auto fs = std::max(8000, static_cast<int>(std::llround(sampleRate)));
    const auto period = std::clamp(framePeriodMs, 1.0, 20.0);
    const auto targetFrames = GetSamplesForHarvest(fs, targetSamples, period);
    if (targetFrames <= 0) return {};

    struct PartAnalysis {
        const NsfHifiganSourcePart* part = nullptr;
        int fs = 0, fft = 0, frames = 0;
        double duration = 0.0;
        std::vector<double> times, f0;
        std::vector<std::vector<double>> spectrum, ap;
    };
    std::vector<PartAnalysis> parts;
    for (const auto& part : assembly)
    {
        if (!part.source || part.source->getNumSamples() <= 0 || part.source->getNumChannels() <= 0
            || !std::isfinite(part.sampleRate) || part.sampleRate <= 0.0
            || !std::isfinite(part.supportFrom) || !std::isfinite(part.supportTo)
            || part.supportTo <= part.supportFrom) continue;
        PartAnalysis item;
        item.part = &part;
        item.fs = std::max(8000, static_cast<int>(std::llround(part.sampleRate)));
        item.duration = static_cast<double>(part.source->getNumSamples()) / item.fs;
        HarvestOption ho; InitializeHarvestOption(&ho); ho.frame_period = period;
        ho.f0_floor = 40.0; ho.f0_ceil = std::min(1600.0, item.fs * .45);
        item.frames = GetSamplesForHarvest(item.fs, part.source->getNumSamples(), period);
        if (item.frames <= 0) continue;
        item.times.resize(static_cast<size_t>(item.frames)); item.f0.resize(item.times.size());
        std::vector<double> mono(static_cast<size_t>(part.source->getNumSamples()));
        for (int s = 0; s < part.source->getNumSamples(); ++s) {
            double v = 0.0; for (int c = 0; c < part.source->getNumChannels(); ++c) v += part.source->getSample(c, s);
            mono[static_cast<size_t>(s)] = v / part.source->getNumChannels();
        }
        Harvest(mono.data(), part.source->getNumSamples(), item.fs, &ho,
                item.times.data(), item.f0.data());
        CheapTrickOption co; InitializeCheapTrickOption(item.fs, &co);
        co.f0_floor = ho.f0_floor; co.fft_size = GetFFTSizeForCheapTrick(item.fs, &co);
        item.fft = co.fft_size;
        const auto bins = item.fft / 2 + 1;
        item.spectrum.assign(item.frames, std::vector<double>(static_cast<size_t>(bins)));
        item.ap.assign(item.frames, std::vector<double>(static_cast<size_t>(bins)));
        std::vector<double*> sp(item.frames), ap(item.frames);
        for (int f = 0; f < item.frames; ++f) { sp[f] = item.spectrum[f].data(); ap[f] = item.ap[f].data(); }
        D4COption d4; InitializeD4COption(&d4);
        CheapTrick(mono.data(), part.source->getNumSamples(), item.fs, item.times.data(), item.f0.data(), item.frames, &co, sp.data());
        D4C(mono.data(), part.source->getNumSamples(), item.fs, item.times.data(), item.f0.data(), item.frames, item.fft, &d4, ap.data());
        parts.push_back(std::move(item));
    }
    if (parts.empty()) return {};
    CheapTrickOption outputCheapTrick; InitializeCheapTrickOption(fs, &outputCheapTrick);
    outputCheapTrick.fft_size = GetFFTSizeForCheapTrick(fs, &outputCheapTrick);
    const auto outBins = outputCheapTrick.fft_size / 2 + 1;
    // WORLD spectra are amplitudes. Accumulate gain^2 in power, then sqrt once;
    // this is deliberately not NSF's Mel-amplitude rule.
    std::vector<std::vector<double>> power(static_cast<size_t>(targetFrames), std::vector<double>(static_cast<size_t>(outBins), 0.0));
    std::vector<std::vector<double>> apPower(static_cast<size_t>(targetFrames), std::vector<double>(static_cast<size_t>(outBins), 0.0));
    std::vector<double> weights(static_cast<size_t>(targetFrames), 0.0), f0Weights(static_cast<size_t>(targetFrames), 0.0), targetF0(static_cast<size_t>(targetFrames), 0.0);
    std::vector<bool> active(static_cast<size_t>(targetFrames), false);
    const auto mapAt = [](const NsfHifiganSourcePart& p, double t) {
        if (p.timeMap.size() < 2) return t;
        auto r = std::upper_bound(p.timeMap.begin(), p.timeMap.end(), t,
            [](double x, const NsfHifiganTimeMapPoint& q) { return x < q.targetSeconds; });
        if (r == p.timeMap.begin()) return r->sourceSeconds;
        if (r == p.timeMap.end()) return (r - 1)->sourceSeconds;
        const auto& a = *(r - 1); const auto& b = *r;
        const auto u = b.targetSeconds > a.targetSeconds ? std::clamp((t-a.targetSeconds)/(b.targetSeconds-a.targetSeconds),0.0,1.0) : 0.0;
        return a.sourceSeconds + (b.sourceSeconds-a.sourceSeconds)*u;
    };
    for (int tf = 0; tf < targetFrames; ++tf) {
        const double t = tf * period / 1000.0;
        for (const auto& item : parts) {
            const auto& p = *item.part;
            if (t < p.supportFrom || t >= p.supportTo) continue;
            const double g = p.gainAt ? std::max(0.0f, p.gainAt(t)) : 1.0;
            if (g <= 0.0) continue;
            const double sf = std::clamp(mapAt(p, t) * 1000.0 / period, 0.0, double(item.frames - 1));
            const auto l = static_cast<size_t>(std::floor(sf)); const auto r = std::min(l + 1, item.spectrum.size()-1); const auto u = sf-l;
            const double sourceF0 = item.f0[l] + (item.f0[r]-item.f0[l])*u;
            const double midi = p.sourceMidiAt ? p.sourceMidiAt(t) : 0.0f;
            // sourceMidiAt marks source voicing for the common F0, but an
            // unvoiced source still contributes its measured D4C noise/AP.
            // This preserves consonants and UV material instead of dropping it.
            const bool sourceVoiced=midi>0.0&&std::isfinite(midi);
            active[static_cast<size_t>(tf)] = true;
            const double w = g*g; weights[static_cast<size_t>(tf)] += w;
            if (sourceVoiced&&sourceF0>0.0) { targetF0[static_cast<size_t>(tf)] += w*sourceF0; f0Weights[static_cast<size_t>(tf)] += w; }
            const double formant = curveAt(formantSemitones, t*1000.0/period, 0.0);
            const double formantRatio = std::pow(2.0, formant / 12.0);
            for (int b = 0; b < outBins; ++b) {
                const double hz = b * double(fs) / (2.0 * outBins - 2.0);
                const double sb = (hz / std::max(1.0e-6, formantRatio)) * item.fft / item.fs;
                const auto sb0 = static_cast<size_t>(std::min(std::max(0.0, std::floor(sb)), double(item.spectrum[l].size()-1)));
                const auto sb1 = std::min(sb0+1, item.spectrum[l].size()-1); const auto bu = sb-sb0;
                const auto amp0 = item.spectrum[l][sb0] + (item.spectrum[l][sb1]-item.spectrum[l][sb0])*bu;
                const auto amp1 = item.spectrum[r][sb0] + (item.spectrum[r][sb1]-item.spectrum[r][sb0])*bu;
                const auto amp = amp0 + (amp1-amp0)*u;
                const auto a0 = item.ap[l][sb0] + (item.ap[l][sb1]-item.ap[l][sb0])*bu;
                const auto a1 = item.ap[r][sb0] + (item.ap[r][sb1]-item.ap[r][sb0])*bu;
                power[static_cast<size_t>(tf)][static_cast<size_t>(b)] += w * amp * amp;
                apPower[static_cast<size_t>(tf)][static_cast<size_t>(b)] += w * amp * amp * std::clamp(a0+(a1-a0)*u, 0.001, 0.999999);
            }
        }
        if (f0Weights[static_cast<size_t>(tf)] > 0.0) targetF0[static_cast<size_t>(tf)] /= f0Weights[static_cast<size_t>(tf)];
        const auto tm = curveAt(targetMidi, t*1000.0/period, 0.0);
        if (targetF0[static_cast<size_t>(tf)] > 0.0 && tm > 0.0)
            targetF0[static_cast<size_t>(tf)] = 440.0 * std::pow(2.0, (tm-69.0)/12.0);
        if (!active[static_cast<size_t>(tf)]) targetF0[static_cast<size_t>(tf)] = 0.0;
    }
    const int fft = outputCheapTrick.fft_size; std::vector<std::vector<double>> spec(targetFrames, std::vector<double>(outBins)), ap(targetFrames, std::vector<double>(outBins));
    std::vector<double*> spp(targetFrames), app(targetFrames);
    for (int f=0; f<targetFrames; ++f) for (int b=0;b<outBins;++b) { spec[f][b]=std::sqrt(std::max(0.0,power[f][b])); ap[f][b]=weights[f]>0?std::clamp(apPower[f][b]/std::max(1e-20,power[f][b]),.001,.999999):.999999; spp[f]=spec[f].data(); app[f]=ap[f].data(); }
    std::vector<double> out(static_cast<size_t>(targetSamples)), noise(static_cast<size_t>(targetSamples));
    SynthesisWithNoiseOutput(targetF0.data(), targetFrames, spp.data(), app.data(), fft, period, fs, targetSamples, out.data(), noiseOutput ? noise.data() : nullptr);
    juce::AudioBuffer<float> result(1, targetSamples); result.clear();
    if (noiseOutput) noiseOutput->setSize(1, targetSamples, false, true, true);
    for(int s=0;s<targetSamples;++s){ const auto f=std::min(targetFrames-1,int(s/(sampleRate*period/1000.0))); result.setSample(0,s,active[static_cast<size_t>(f)]&&std::isfinite(out[s])?float(out[s]):0.f); if(noiseOutput) noiseOutput->setSample(0,s,active[static_cast<size_t>(f)]&&std::isfinite(noise[s])?float(noise[s]):0.f); }
    return result;
}

juce::AudioBuffer<float> WorldRenderer::render(
    const juce::AudioBuffer<float>& source,
    int targetSamples,
    double sampleRate,
    double framePeriodMs,
    const std::vector<float>& sourceMidi,
    const std::vector<float>& targetMidi,
    const std::vector<float>& formantSemitones,
    const std::vector<WorldTimeMapPoint>& timeMap,
    juce::AudioBuffer<float>* noiseOutput)
{
    if (noiseOutput != nullptr) noiseOutput->setSize(0, 0);
    const auto sourceSamples = source.getNumSamples();
    const auto channels = source.getNumChannels();
    if (sourceSamples <= 0 || targetSamples <= 0 || channels <= 0 || sampleRate <= 0.0)
        return {};

    auto timeChanged = sourceSamples != targetSamples;
    const auto sourceDuration = static_cast<double>(sourceSamples) / sampleRate;
    const auto targetDuration = static_cast<double>(targetSamples) / sampleRate;
    for (const auto& point : timeMap)
        timeChanged = timeChanged || std::abs(
            point.sourceSeconds / std::max(1.0e-9, sourceDuration)
            - point.targetSeconds / std::max(1.0e-9, targetDuration)) > 1.0e-6;
    const auto formantChanged = std::any_of(formantSemitones.begin(), formantSemitones.end(),
        [](float value) { return std::abs(value) > 1.0e-4f; });
    if (noiseOutput == nullptr && !timeChanged && !formantChanged && !hasPitchDifference(sourceMidi, targetMidi))
        return source;

    const auto fs = std::max(8'000, static_cast<int>(std::llround(sampleRate)));
    const auto period = std::clamp(framePeriodMs, 1.0, 20.0);
    HarvestOption harvestOption;
    InitializeHarvestOption(&harvestOption);
    harvestOption.frame_period = period;
    harvestOption.f0_floor = 40.0;
    harvestOption.f0_ceil = std::min(1'600.0, sampleRate * 0.45);
    const auto sourceFrames = GetSamplesForHarvest(fs, sourceSamples, period);
    const auto targetFrames = GetSamplesForHarvest(fs, targetSamples, period);
    if (sourceFrames <= 0 || targetFrames <= 0) return {};

    std::vector<double> analysisMono(static_cast<std::size_t>(sourceSamples), 0.0);
    for (int sample = 0; sample < sourceSamples; ++sample)
    {
        double sum = 0.0;
        for (int channel = 0; channel < channels; ++channel)
            sum += source.getSample(channel, sample);
        analysisMono[static_cast<std::size_t>(sample)] = sum / channels;
    }
    std::vector<double> temporalPositions(static_cast<std::size_t>(sourceFrames), 0.0);
    std::vector<double> analysedF0(static_cast<std::size_t>(sourceFrames), 0.0);
    Harvest(analysisMono.data(), sourceSamples, fs, &harvestOption,
            temporalPositions.data(), analysedF0.data());

    CheapTrickOption cheapTrickOption;
    InitializeCheapTrickOption(fs, &cheapTrickOption);
    cheapTrickOption.f0_floor = harvestOption.f0_floor;
    cheapTrickOption.fft_size = GetFFTSizeForCheapTrick(fs, &cheapTrickOption);
    const auto bins = cheapTrickOption.fft_size / 2 + 1;
    D4COption d4cOption;
    InitializeD4COption(&d4cOption);

    std::vector<double> targetF0(static_cast<std::size_t>(targetFrames), 0.0);
    std::vector<double> sourceFramePositions(static_cast<std::size_t>(targetFrames), 0.0);
    for (int frame = 0; frame < targetFrames; ++frame)
    {
        const auto targetTime = std::min(targetDuration, frame * period / 1000.0);
        const auto sourceTime = sourceTimeAt(timeMap, targetTime, sourceDuration, targetDuration);
        const auto sourceFrame = std::clamp(sourceTime * 1000.0 / period,
                                             0.0, static_cast<double>(sourceFrames - 1));
        sourceFramePositions[static_cast<std::size_t>(frame)] = sourceFrame;
        const auto left = static_cast<std::size_t>(std::floor(sourceFrame));
        const auto right = std::min(left + 1, analysedF0.size() - 1);
        const auto amount = sourceFrame - static_cast<double>(left);
        const auto sourceF0 = analysedF0[left] + (analysedF0[right] - analysedF0[left]) * amount;
        const auto desiredMidi = curveAt(targetMidi, targetTime * 1000.0 / period, 0.0);
        // Keep consonants, breaths and sibilants unvoiced even when a note blob
        // spans them.  Voiced frames follow the imported/edited contour exactly.
        if (sourceF0 > 0.0 && desiredMidi > 0.0)
            targetF0[static_cast<std::size_t>(frame)] =
                440.0 * std::pow(2.0, (desiredMidi - 69.0) / 12.0);
        else
            targetF0[static_cast<std::size_t>(frame)] = sourceF0 > 0.0 ? sourceF0 : 0.0;
    }

    juce::AudioBuffer<float> output(channels, targetSamples);
    output.clear();
    if (noiseOutput != nullptr)
    {
        noiseOutput->setSize(channels, targetSamples);
        noiseOutput->clear();
    }
    for (int channel = 0; channel < channels; ++channel)
    {
        std::vector<double> input(static_cast<std::size_t>(sourceSamples));
        for (int sample = 0; sample < sourceSamples; ++sample)
            input[static_cast<std::size_t>(sample)] = source.getSample(channel, sample);

        std::vector<std::vector<double>> spectrum(static_cast<std::size_t>(sourceFrames),
                                                   std::vector<double>(static_cast<std::size_t>(bins)));
        std::vector<std::vector<double>> aperiodicity(static_cast<std::size_t>(sourceFrames),
                                                       std::vector<double>(static_cast<std::size_t>(bins)));
        std::vector<double*> spectrumPointers(static_cast<std::size_t>(sourceFrames));
        std::vector<double*> aperiodicityPointers(static_cast<std::size_t>(sourceFrames));
        for (int frame = 0; frame < sourceFrames; ++frame)
        {
            spectrumPointers[static_cast<std::size_t>(frame)] = spectrum[static_cast<std::size_t>(frame)].data();
            aperiodicityPointers[static_cast<std::size_t>(frame)] = aperiodicity[static_cast<std::size_t>(frame)].data();
        }
        CheapTrick(input.data(), sourceSamples, fs, temporalPositions.data(), analysedF0.data(),
                   sourceFrames, &cheapTrickOption, spectrumPointers.data());
        D4C(input.data(), sourceSamples, fs, temporalPositions.data(), analysedF0.data(),
            sourceFrames, cheapTrickOption.fft_size, &d4cOption, aperiodicityPointers.data());

        std::vector<std::vector<double>> warpedSpectrum(static_cast<std::size_t>(targetFrames),
                                                         std::vector<double>(static_cast<std::size_t>(bins)));
        std::vector<std::vector<double>> warpedAperiodicity(static_cast<std::size_t>(targetFrames),
                                                             std::vector<double>(static_cast<std::size_t>(bins)));
        std::vector<const double*> warpedSpectrumPointers(static_cast<std::size_t>(targetFrames));
        std::vector<const double*> warpedAperiodicityPointers(static_cast<std::size_t>(targetFrames));
        for (int frame = 0; frame < targetFrames; ++frame)
        {
            const auto targetTime = std::min(targetDuration, frame * period / 1000.0);
            const auto formant = curveAt(formantSemitones, targetTime * 1000.0 / period, 0.0);
            const auto formantRatio = std::pow(2.0, formant / 12.0);
            for (int bin = 0; bin < bins; ++bin)
            {
                warpedSpectrum[static_cast<std::size_t>(frame)][static_cast<std::size_t>(bin)] =
                    matrixAt(spectrum, sourceFramePositions[static_cast<std::size_t>(frame)],
                             static_cast<double>(bin) / formantRatio, true);
                warpedAperiodicity[static_cast<std::size_t>(frame)][static_cast<std::size_t>(bin)] =
                    std::clamp(matrixAt(aperiodicity,
                                        sourceFramePositions[static_cast<std::size_t>(frame)],
                                        static_cast<double>(bin), false), 0.001, 0.999999999999);
            }
            warpedSpectrumPointers[static_cast<std::size_t>(frame)] =
                warpedSpectrum[static_cast<std::size_t>(frame)].data();
            warpedAperiodicityPointers[static_cast<std::size_t>(frame)] =
                warpedAperiodicity[static_cast<std::size_t>(frame)].data();
        }

        std::vector<double> synthesized(static_cast<std::size_t>(targetSamples), 0.0);
        std::vector<double> noise(noiseOutput != nullptr ? static_cast<std::size_t>(targetSamples) : 0);
        SynthesisWithNoiseOutput(targetF0.data(), targetFrames, warpedSpectrumPointers.data(),
                  warpedAperiodicityPointers.data(), cheapTrickOption.fft_size,
                  period, fs, targetSamples, synthesized.data(), noise.empty() ? nullptr : noise.data());
        auto* destination = output.getWritePointer(channel);
        for (int sample = 0; sample < targetSamples; ++sample)
        {
            const auto value = synthesized[static_cast<std::size_t>(sample)];
            destination[sample] = std::isfinite(value) ? static_cast<float>(value) : 0.0f;
            if (noiseOutput != nullptr)
                noiseOutput->setSample(channel, sample, std::isfinite(noise[sample]) ? static_cast<float>(noise[sample]) : 0.0f);
        }
    }
    return output;
}
}
