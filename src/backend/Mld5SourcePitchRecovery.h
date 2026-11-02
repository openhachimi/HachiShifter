#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace hachi::backend
{
// Render-only, deliberately conservative repair of caller-authorised missing
// pitch frames. Neither project pitch/voicing nor a coarse property/period is
// evidence here: a hint restricts the search, but only local PCM measures F0.
struct Mld5SourcePitchRecovery
{
    struct RecoveryFrame
    {
        std::size_t index = 0;
        double sourceSeconds = 0;
        float sourceMidi = 0;
        float targetMidi = 0;
        float confidence = 0; // measured normalised autocorrelation, not a model probability
    };

    static constexpr std::size_t maxFrames = 24000;
    static constexpr int maxWindowSamples = 16384;
    static constexpr double minRate = 8000;
    static constexpr double maxRate = 192000;
    static constexpr double minHz = 55;
    static constexpr double maxHz = 1600;
    static constexpr double minRms = 1.e-4; // -80 dBFS; absolute AC energy, after DC removal
    static constexpr double minCorrelation = .97;

    // All four MIDI vectors are on the SAME target grid, starting at t=0, and
    // must have identical sizes (no broadcasting, interpolation or forward fill).
    // sourceAt(t) returns seconds LOCAL TO pcm, not original-file absolute time.
    // It is evaluated exactly once for each otherwise eligible frame. This helper
    // neither resamples PCM nor adjusts/clamps the source clock. The rate must be
    // the actual native PCM rate, not the MLD5 DSP/principal rate.
    //
    // Caller supplies hint>0 only for an unknown gap with an independent known
    // source pitch in the same note, and intent>0 only for explicit target intent.
    // Explicit silence, nativeUnpitched, no measured reference, etc. MUST pass 0.
    // An all-zero pitch vector alone NEVER grants permission to recover it.
    // Both existing source AND target must be exactly zero; any existing value,
    // including a partially known pair, is left alone. Target is copied from the
    // caller's intent, NEVER from the newly measured source pitch or the hint.
    //
    // Bounds: 1..24000 frames, .001..1 second frame step, 8..192 kHz PCM,
    // 55..1600 Hz hint, (0,127] MIDI intent, <=16384 native window samples.
    // Invalid global inputs return no repairs; invalid local evidence skips that
    // frame. A full >=40 ms / >=4 longest candidate periods is required; at a
    // physical edge only the ANALYSIS WINDOW shifts inward within this PCM.
    // The queried clock never moves. Additional local >=3-period correlation
    // and nearest-period AC energy must confirm voice at that queried location.
    // No edge padding, short-buffer extrapolation or UV fill.
    // Cancellation may throw; output edits are committed only after ALL analysis
    // succeeds, so cancellation or a throwing sourceAt leaves both outputs intact.
    [[nodiscard]] static std::vector<RecoveryFrame> recover(
        const std::vector<float>& pcm, double rate, double frameSeconds,
        const std::function<double(double)>& sourceAt,
        const std::vector<float>& sourceHintMidi,
        const std::vector<float>& targetIntentMidi,
        std::vector<float>& sourceMidi, std::vector<float>& targetMidi,
        const std::function<void()>& checkCancellation = {})
    {
        std::vector<RecoveryFrame> recovered;
        const auto count = sourceMidi.size();
        if (pcm.empty() || count == 0 || count > maxFrames || !sourceAt
            || targetMidi.size() != count || sourceHintMidi.size() != count
            || targetIntentMidi.size() != count || &sourceMidi == &targetMidi
            || &pcm == &sourceMidi || &pcm == &targetMidi
            || !std::isfinite(rate) || rate < minRate || rate > maxRate
            || !std::isfinite(frameSeconds) || frameSeconds < .001 || frameSeconds > 1)
            return recovered;

        std::unique_ptr<juce::dsp::FFT> fft;
        int activeOrder = 0;
        std::vector<std::complex<float>> input, spectrum;
        std::vector<double> energy;
        const auto lastSourceSeconds = double(pcm.size() - 1) / rate;
        for (std::size_t frame = 0; frame < count; ++frame)
        {
            if (checkCancellation) checkCancellation();
            const auto hint = double(sourceHintMidi[frame]);
            const auto intent = targetIntentMidi[frame];
            if (sourceMidi[frame] != 0 || targetMidi[frame] != 0
                || !std::isfinite(hint) || hint <= 0 || hint > 127
                || !std::isfinite(intent) || intent <= 0 || intent > 127)
                continue;
            const auto hintHz = 440 * std::exp2((hint - 69) / 12);
            if (hintHz < minHz || hintHz > maxHz) continue;

            // +/-25% PERIOD, excluding neighbouring octaves. Search guards on
            // either side support a real interior peak and fractional lag fit.
            const auto hintLag = rate / hintHz;
            const auto lowPeriod = std::max(rate / maxHz, hintLag * .75);
            const auto highPeriod = std::min(rate / minHz, hintLag * 1.25);
            const auto firstLag = std::max(2, int(std::floor(lowPeriod)));
            const auto lastLag = int(std::ceil(highPeriod));
            const auto n = int(std::ceil(std::max(.04 * rate, 4.0 * (lastLag + 1))));
            if (lastLag <= firstLag + 1 || n > maxWindowSamples || n <= lastLag + 1)
                continue;

            const auto seconds = sourceAt(double(frame) * frameSeconds);
            if (!std::isfinite(seconds) || seconds < 0 || seconds > lastSourceSeconds)
                continue;
            const auto centre = std::size_t(std::floor(seconds * rate));
            if (centre >= pcm.size() || std::size_t(n) > pcm.size()) continue;
            const auto windowStart = [&](std::size_t length)
            {
                const auto half = length / 2;
                return std::min(centre >= half ? centre - half : std::size_t(0),
                                pcm.size() - length);
            };
            const auto start = windowStart(std::size_t(n));

            double mean = 0;
            bool finite = true;
            for (int i = 0; i < n; ++i)
            {
                const auto value = pcm[start + std::size_t(i)];
                if (!std::isfinite(value)) { finite = false; break; }
                mean += value;
            }
            if (!finite) continue;
            mean /= n;
            int order = 1;
            while ((1 << order) < 2 * n) ++order; // <=32768; zero padding avoids circular ACF
            if (order != activeOrder)
            {
                fft = std::make_unique<juce::dsp::FFT>(order);
                activeOrder = order;
                input.resize(std::size_t(1 << order));
                spectrum.resize(input.size());
            }
            std::fill(input.begin(), input.end(), std::complex<float>{});
            energy.resize(std::size_t(n + 1));
            energy[0] = 0;
            for (int i = 0; i < n; ++i)
            {
                const auto value = double(pcm[start + std::size_t(i)]) - mean;
                input[std::size_t(i)] = { float(value), 0 };
                energy[std::size_t(i + 1)] = energy[std::size_t(i)] + value * value;
            }
            if (energy.back() / n < minRms * minRms) continue;
            // A constant/DC signal is not voiced. Reject implausibly large PCM
            // before float FFT power can overflow; never normalise it into voice.
            if (!std::isfinite(energy.back()) || energy.back() / n > 16) continue;
            fft->perform(input.data(), spectrum.data(), false);
            for (auto& value : spectrum) value = { std::norm(value), 0 };
            fft->perform(spectrum.data(), input.data(), true);
            const auto correlation = [&](int lag)
            {
                const auto left = energy[std::size_t(n - lag)];
                const auto right = energy.back() - energy[std::size_t(lag)];
                if (left <= 0 || right <= 0) return -1.0;
                return double(input[std::size_t(lag)].real()) / std::sqrt(left * right);
            };
            double best = -1;
            int peak = 0;
            for (int lag = firstLag; lag <= lastLag; ++lag)
            {
                const auto value = correlation(lag);
                if (std::isfinite(value) && value > best) { best = value; peak = lag; }
            }
            if (peak <= firstLag || peak >= lastLag || best < minCorrelation) continue;
            const auto previous = correlation(peak - 1), next = correlation(peak + 1);
            const auto denominator = previous - 2 * best + next;
            if (!std::isfinite(previous) || !std::isfinite(next) || denominator >= -1.e-12)
                continue;
            const auto delta = std::clamp(.5 * (previous - next) / denominator, -.5, .5);
            const auto lag = double(peak) + delta;
            if (lag < lowPeriod || lag > highPeriod) continue;
            // A strong subperiod means the hint may be an octave/subharmonic
            // error: bounded search alone must not relabel 840 Hz as 420 Hz.
            // Reject ambiguity rather than widening the search or trusting hint.
            bool shorterPeriod = false;
            for (int divisor = 2; divisor <= 3; ++divisor)
            {
                const auto sub = lag / divisor;
                if (sub < rate / maxHz) continue;
                const auto a = int(std::floor(sub)), b = int(std::ceil(sub));
                if (a >= 2 && std::max(correlation(a), correlation(b)) >= minCorrelation)
                    shorterPeriod = true;
            }
            if (shorterPeriod) continue;

            // A shifted full window must not borrow a preceding vowel to make
            // silence/frication at the actual query location appear voiced.
            // Recheck >=3 measured cycles locally and a nearest-cycle AC energy
            // window. Both stay inside pcm; neither changes reported seconds.
            const auto localN = std::size_t(std::ceil(3 * lag)) + 2;
            const auto nearN = std::size_t(std::ceil(lag)) + 1;
            if (localN > pcm.size() || nearN > pcm.size()) continue;
            const auto localStart = windowStart(localN), nearStart = windowStart(nearN);
            double nearMean = 0, localMean = 0;
            for (std::size_t i = 0; i < nearN; ++i) nearMean += pcm[nearStart + i];
            for (std::size_t i = 0; i < localN; ++i) localMean += pcm[localStart + i];
            nearMean /= double(nearN); localMean /= double(localN);
            double nearEnergy = 0, localEnergy = 0;
            for (std::size_t i = 0; i < nearN; ++i)
            {
                const auto x = double(pcm[nearStart + i]) - nearMean;
                nearEnergy += x * x;
            }
            for (std::size_t i = 0; i < localN; ++i)
            {
                const auto x = double(pcm[localStart + i]) - localMean;
                localEnergy += x * x;
            }
            if (!std::isfinite(nearEnergy) || !std::isfinite(localEnergy)
                || nearEnergy / double(nearN) < minRms * minRms
                || localEnergy / double(localN) < minRms * minRms) continue;
            double cross = 0, left = 0, right = 0;
            for (std::size_t i = 0; i + std::size_t(peak) < localN; ++i)
            {
                const auto a = double(pcm[localStart + i]) - localMean;
                const auto b = double(pcm[localStart + i + std::size_t(peak)]) - localMean;
                cross += a * b; left += a * a; right += b * b;
            }
            const auto localCorrelation = left > 0 && right > 0
                ? cross / std::sqrt(left * right) : -1;
            if (!std::isfinite(localCorrelation) || localCorrelation < minCorrelation) continue;
            const auto hz = rate / lag;
            const auto midi = 69 + 12 * std::log2(hz / 440);
            if (!std::isfinite(midi) || midi <= 0 || midi > 127) continue;
            recovered.push_back({ frame, seconds, float(midi), intent,
                                  float(std::clamp(std::min(best, localCorrelation), 0.0, 1.0)) });
        }
        if (checkCancellation) checkCancellation();
        for (const auto& frame : recovered)
        {
            sourceMidi[frame.index] = frame.sourceMidi;
            targetMidi[frame.index] = frame.targetMidi;
        }
        return recovered;
    }
};
}
