#include "packed_fft.hpp"
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

#if defined(__FAST_MATH__)
#error "Spectral contract arithmetic must not use fast-math"
#endif
namespace melodyne_re {
namespace {
PackedFftResult reject(const char* why) {PackedFftResult r;r.error=why;return r;}
const char* validate(const std::vector<float>& input,bool unaligned) {
    if(unaligned)return "native unaligned wrapper branch is unsupported";
    const auto n=input.size();
    if(n<8 || n>4096 || (n&(n-1)))return "supported FFT lengths are powers of two from8 through4096";
    for(float v:input)if(!std::isfinite(v))return "nonfinite FFT input";
    return nullptr;
}
// Standard Cooley-Tukey radix2 is a design choice, NOT recovered native code.
// Forward negative exponential; backward positive, both unnormalized.
void fft(std::vector<std::complex<float>>& data,bool backward) {
    const std::size_t n=data.size();
    for(std::size_t i=1,j=0;i<n;++i) {
        auto bit=n>>1;
        for(;j&bit;bit>>=1)j^=bit;
        j^=bit;
        if(i<j)std::swap(data[i],data[j]);
    }
    constexpr double tau=6.283185307179586476925286766559;
    for(std::size_t width=2;width<=n;width<<=1) {
        const auto half=width>>1;
        for(std::size_t j=0;j<half;++j) {
            const double angle=(backward?tau:-tau)*static_cast<double>(j)/static_cast<double>(width);
            const std::complex<float> twiddle(static_cast<float>(std::cos(angle)),static_cast<float>(std::sin(angle)));
            for(std::size_t start=0;start<n;start+=width) {
                const auto a=data[start+j];
                const auto b=data[start+j+half]*twiddle;
                data[start+j]=a+b;
                data[start+j+half]=a-b;
            }
        }
    }
}
}
PackedFftResult packed_real_forward_math(const std::vector<float>& input,bool unaligned) {
    if(const char* error=validate(input,unaligned))return reject(error);
    const auto n=input.size();
    std::vector<std::complex<float>> spectrum(n);
    for(std::size_t i=0;i<n;++i)spectrum[i]={input[i],0.0f};
    fft(spectrum,false);
    PackedFftResult r;r.values.resize(n);
    const float positive=2.0f/static_cast<float>(n);
    const float negative=-2.0f/static_cast<float>(n);
    r.values[0]=positive*spectrum[0].real();
    r.values[1]=negative*spectrum[n/2].real();
    for(std::size_t k=1;k<n/2;++k) {
        r.values[2*k]=positive*spectrum[k].real();
        r.values[2*k+1]=negative*spectrum[k].imag();
    }
    for(float x:r.values)if(!std::isfinite(x))return reject("FFT arithmetic overflow");
    r.ok=true;return r;
}
PackedFftResult packed_real_inverse_math(const std::vector<float>& input,bool unaligned) {
    if(const char* error=validate(input,unaligned))return reject(error);
    const auto n=input.size();
    std::vector<std::complex<float>> spectrum(n);
    spectrum[0]={input[0]*0.5f,0.0f};
    spectrum[n/2]={input[1]*-0.5f,0.0f};
    for(std::size_t k=1;k<n/2;++k) {
        spectrum[k]={input[2*k]*0.5f,input[2*k+1]*-0.5f};
        spectrum[n-k]=std::conj(spectrum[k]);
    }
    fft(spectrum,true);
    PackedFftResult r;r.values.resize(n);
    for(std::size_t i=0;i<n;++i) {
        r.values[i]=spectrum[i].real();
        if(!std::isfinite(r.values[i]))return reject("inverse FFT arithmetic overflow");
    }
    r.ok=true;return r;
}
} // namespace melodyne_re
