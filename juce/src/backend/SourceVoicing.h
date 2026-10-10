#pragma once
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <vector>

namespace hachi::backend
{
// Independent source evidence: a pitch model's bin confidence is not a
// voiced/unvoiced probability. Only reject clearly aperiodic windows; keep
// uncertain/breathy vowels rather than turning their pitched component off.
struct SourceVoicing
{
    static constexpr double period = .01;
    std::vector<float> unvoiced;

    float at(double seconds) const
    {
        if (unvoiced.empty()) return 0;
        const auto pos = juce::jlimit(0.0, double(unvoiced.size()-1), seconds / period);
        const auto i = std::size_t(pos), j = std::min(i+1, unvoiced.size()-1);
        return unvoiced[i] + (unvoiced[j]-unvoiced[i]) * float(pos-i);
    }

    static SourceVoicing analyse(const std::vector<float>& source, double rate,
                                const std::function<void()>& checkCancellation = {})
    {
        SourceVoicing result;
        if (source.empty() || rate <= 0) return result;
        constexpr double analysisRate = 16000;
        constexpr int window = 640, fftSize = 2048;
        const auto duration = double(source.size()) / rate;
        // Short transients cannot support a reliable periodicity decision.
        if (duration < .02) return result;
        std::vector<float> mono(std::size_t(std::ceil(duration*analysisRate)));
        for (std::size_t i=0; i<mono.size(); ++i)
        {
            const auto pos = std::min(double(source.size()-1), i*rate/analysisRate);
            const auto a = std::size_t(pos), b = std::min(a+1,source.size()-1);
            mono[i] = source[a] + (source[b]-source[a])*float(pos-a);
        }
        juce::dsp::FFT fft(11);
        std::vector<std::complex<float>> input(fftSize), spectrum(fftSize);
        std::vector<double> energy(window+1);
        const auto count = std::size_t(std::ceil(duration/period))+1;
        std::vector<bool> noise(count);
        result.unvoiced.assign(count,0);
        for (std::size_t frame=0; frame<count; ++frame)
        {
            if (checkCancellation) checkCancellation();
            const auto start = juce::jlimit(0, std::max(0,int(mono.size())-window),
                int(std::llround(frame*period*analysisRate))-window/2);
            const auto n = std::min(window,int(mono.size())-start);
            double mean=0;
            for (int i=0;i<n;++i) mean+=mono[std::size_t(start+i)];
            mean/=std::max(1,n);
            std::fill(input.begin(),input.end(),std::complex<float>{});
            energy[0]=0;
            for (int i=0;i<n;++i)
            {
                const auto value=float(mono[std::size_t(start+i)]-mean);
                input[std::size_t(i)]={value,0};
                energy[std::size_t(i+1)]=energy[std::size_t(i)]+double(value)*value;
            }
            if (energy[std::size_t(n)]/std::max(1,n)<1.e-10)
            { noise[frame]=true; continue; }
            fft.perform(input.data(),spectrum.data(),false);
            // Frication can contain coloured/weakly correlated noise. Use
            // broadband/high-frequency evidence as well as periodicity;
            // never reject a periodic breathy vowel merely for being noisy.
            double totalPower=0, highPower=0, logPower=0;
            constexpr int firstBin=8, highBin=256, lastBin=fftSize/2;
            for(int bin=firstBin;bin<=lastBin;++bin)
            {
                const auto power=std::max(1.e-20,double(std::norm(spectrum[std::size_t(bin)])));
                totalPower+=power; logPower+=std::log(power);
                if(bin>=highBin) highPower+=power;
            }
            const auto flatness=std::exp(logPower/(lastBin-firstBin+1))
                / std::max(1.e-20,totalPower/(lastBin-firstBin+1));
            const auto highRatio=highPower/std::max(1.e-20,totalPower);
            for (auto& value:spectrum) value={std::norm(value),0};
            fft.perform(spectrum.data(),input.data(),true);
            double best=0;
            // 50--1000 Hz covers vocal fundamentals; ignore lag zero and
            // short-lag colour correlation in friction noise.
            for (int lag=16;lag<=std::min(320,n/2);++lag)
            {
                const auto denominator=std::sqrt(std::max(1.e-20,
                    energy[std::size_t(n-lag)]*(energy[std::size_t(n)]-energy[std::size_t(lag)])));
                best=std::max(best,double(input[std::size_t(lag)].real())/denominator);
            }
            noise[frame]=best<.25 || (best<.45 && highRatio>.55 && flatness>.04);
        }
        // Require at least two consecutive decisions. Isolated pitch attacks
        // and brief mixed windows remain neural; no forward filling of F0.
        for(std::size_t i=0;i<count;++i)
            if(noise[i] && ((i>0 && noise[i-1]) || (i+1<count && noise[i+1])))
                result.unvoiced[i]=1;
        return result;
    }
};
}
