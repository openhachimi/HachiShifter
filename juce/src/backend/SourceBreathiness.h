#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <functional>
#include <vector>

namespace hachi::backend
{
// Display evidence only. A pitched vowel can contain audible turbulent noise;
// unlike SourceVoicing this must never suppress its F0 or restore its entire PCM.
// Period cancellation estimates the aperiodic residual; broadband evidence
// rejects the narrowband residual of a pitch glide or harmonic interpolation.
struct SourceBreathiness
{
    static constexpr double period = .01;
    std::vector<float> marked;
    std::vector<float> periodicity;
    std::vector<float> noiseFraction;

    static SourceBreathiness analyse(const std::vector<float>& source, double rate,
                                    const std::function<void()>& checkCancellation = {})
    {
        SourceBreathiness result;
        if (source.empty() || !(rate > 0)) return result;
        constexpr double analysisRate = 16000;
        constexpr int window = 640, fftSize = 2048;
        const auto duration = source.size() / rate;
        if (duration < .02) return result;
        std::vector<float> mono(static_cast<std::size_t>(std::ceil(duration * analysisRate)));
        for (std::size_t i = 0; i < mono.size(); ++i)
        {
            const auto position = std::min(double(source.size() - 1), i * rate / analysisRate);
            const auto a = static_cast<std::size_t>(position), b = std::min(a + 1, source.size() - 1);
            mono[i] = source[a] + (source[b] - source[a]) * float(position - a);
        }
        const auto frames = static_cast<std::size_t>(std::ceil(duration / period)) + 1;
        result.marked.assign(frames, 0); result.periodicity.assign(frames, 0); result.noiseFraction.assign(frames, 0);
        juce::dsp::FFT fft(11);
        std::vector<std::complex<float>> input(fftSize), spectrum(fftSize);
        std::array<double, window + 1> energy{};
        std::array<double, 322> correlation{};
        std::vector<float> raw(frames, 0), rms(frames, 0);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            if (checkCancellation) checkCancellation();
            const auto start = juce::jlimit(0, std::max(0, int(mono.size()) - window),
                int(std::llround(frame * period * analysisRate)) - window / 2);
            const auto n = std::min(window, int(mono.size()) - start);
            double mean = 0;
            for (int i = 0; i < n; ++i) mean += mono[std::size_t(start + i)];
            mean /= n;
            std::fill(input.begin(), input.end(), std::complex<float>{}); energy[0] = 0;
            for (int i = 0; i < n; ++i)
            {
                const auto x = mono[std::size_t(start + i)] - float(mean);
                input[std::size_t(i)] = {x, 0}; energy[std::size_t(i + 1)] = energy[std::size_t(i)] + double(x) * x;
            }
            rms[frame] = float(energy[std::size_t(n)] / n);
            if (rms[frame] < 1.e-10f) continue; // silence is not breath evidence
            fft.perform(input.data(), spectrum.data(), false);
            for (auto& x : spectrum) x = {std::norm(x), 0};
            fft.perform(spectrum.data(), input.data(), true);
            const auto lastLag = std::min(320, n / 2);
            int bestLag = 16; double best = 0;
            for (int lag = 15; lag <= lastLag + 1; ++lag)
            {
                const auto denominator = std::sqrt(std::max(1.e-20,
                    energy[std::size_t(n - lag)] * (energy[std::size_t(n)] - energy[std::size_t(lag)])));
                correlation[std::size_t(lag)] = input[std::size_t(lag)].real() / denominator;
                if (lag >= 16 && lag <= lastLag && correlation[std::size_t(lag)] > best)
                { best = correlation[std::size_t(lag)]; bestLag = lag; }
            }
            // Prefer an earlier strong local peak over a long multiple of the
            // period; it leaves more residual samples and follows changing F0.
            for (int lag = 16; lag < bestLag; ++lag)
                if (correlation[std::size_t(lag)] >= best - .01
                    && correlation[std::size_t(lag)] >= correlation[std::size_t(lag - 1)]
                    && correlation[std::size_t(lag)] >= correlation[std::size_t(lag + 1)])
                { bestLag = lag; break; }
            result.periodicity[frame] = float(juce::jlimit(0.0, 1.0, best));
            const auto a = correlation[std::size_t(bestLag - 1)], b = correlation[std::size_t(bestLag)], c = correlation[std::size_t(bestLag + 1)];
            const auto denominator = a - 2 * b + c;
            const auto fraction = std::abs(denominator) > 1.e-12
                ? juce::jlimit(-.5, .5, .5 * (a - c) / denominator) : 0.0;
            const auto lag = bestLag + fraction;
            const auto residualSize = n - int(std::ceil(lag));
            std::fill(input.begin(), input.end(), std::complex<float>{});
            double residualEnergy = 0, originalEnergy = 0;
            for (int i = 0; i < residualSize; ++i)
            {
                const auto position = i + lag; const auto left = int(position); const auto u = float(position - left);
                const auto x = mono[std::size_t(start + i)] - float(mean);
                const auto right = std::min(left + 1, n - 1);
                const auto delayed = mono[std::size_t(start + left)]
                    + (mono[std::size_t(start + right)] - mono[std::size_t(start + left)]) * u - float(mean);
                const auto residual = (x - delayed) * .70710678f;
                residualEnergy += double(residual) * residual; originalEnergy += double(x) * x;
                const auto hann = .5 - .5 * std::cos(juce::MathConstants<double>::twoPi * i / std::max(1, residualSize - 1));
                input[std::size_t(i)] = {float(residual * hann), 0};
            }
            fft.perform(input.data(), spectrum.data(), false);
            // Bands isolate coloured / low-frequency aspiration rather than
            // requiring the old >2 kHz energy majority of a sharp fricative.
            constexpr std::array<std::pair<int,int>, 5> bands {{{10,32},{32,128},{128,256},{256,512},{512,960}}};
            double total = 0, broad = 0;
            for (const auto& [from, to] : bands)
            {
                double sum = 0, logSum = 0;
                for (int bin = from; bin < to; ++bin)
                { const auto power = std::max(1.e-20, double(std::norm(spectrum[std::size_t(bin)]))); sum += power; logSum += std::log(power); }
                const auto flatness = std::exp(logSum / (to - from)) / std::max(1.e-20, sum / (to - from));
                total += sum;
                if (flatness > .18) broad += sum;
            }
            const auto residualRatio = residualEnergy / std::max(1.e-20, originalEnergy);
            const auto broadShare = broad / std::max(1.e-20, total);
            const auto evidence = juce::jlimit(0.0, 1.0, residualRatio * broadShare);
            result.noiseFraction[frame] = float(evidence);
            // Noise can coexist with a stable fundamental. Both residual level
            // and spectral breadth are needed to avoid dashing clean vibrato.
            raw[frame] = evidence >= .025 && broadShare >= .35 ? 1.0f : 0.0f;
            if (best < .35 && broadShare >= .2) raw[frame] = 1; // clear coloured frication
        }
        const auto peak = *std::max_element(rms.begin(), rms.end());
        for (std::size_t i = 0; i < frames; ++i)
        {
            if (rms[i] < std::max(1.e-10f, peak * 1.e-6f)) continue;
            // A brief 10 ms hole inside consistent noise is a mixed window,
            // but never bridge a longer vowel or silence. Reject lone attacks.
            const auto marked = raw[i] > 0 && ((i && raw[i - 1] > 0) || (i + 1 < frames && raw[i + 1] > 0));
            const auto hole = i && i + 1 < frames && raw[i - 1] > 0 && raw[i + 1] > 0;
            if (marked || hole) result.marked[i] = 1;
        }
        return result;
    }
};
}
