#include "spectral_frame.hpp"
#include "component_reconstruction.hpp"
#include "packed_fft.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "Recovered frame arithmetic must not use fast-math"
#endif
namespace melodyne_re {
namespace {
SpectralFrameResult reject(const char* why) {SpectralFrameResult r;r.error=why;return r;}
bool initial_half(std::vector<float>& samples,int begin,int length,
                  const RaisedCosineKernel::Table& table,int offset) {
    if(!length)return true;
    const float step=8192.0f/static_cast<float>(length);
    float clock=0.0f;
    for(int i=0;i<length;++i) {
        if(!std::isfinite(clock) || clock<0 || clock>16384.0f)return false;
        const int index=static_cast<int>(clock)+offset;
        if(index>16384)return false;
        auto& value=samples[static_cast<std::size_t>(begin+i)];
        value=table[static_cast<std::size_t>(index)]*value;
        clock=clock+step;
    }
    return true;
}
// Ordered scalar magnitude branch at 9970, not std::hypot or sqrt(re*re+im*im).
float stable_magnitude(float real,float imaginary) {
    const float a=std::fabs(real),b=std::fabs(imaginary);
    if(a==0.0f)return b;
    if(b==0.0f)return a;
    if(a<=b) {
        const float ratio=a/b;
        const float sum=(ratio*ratio)+1.0f;
        return std::sqrt(sum)*b;
    }
    const float ratio=b/a;
    const float sum=(ratio*ratio)+1.0f;
    return std::sqrt(sum)*a;
}
}
SpectralFrameResult process_spectral_frame_mono(
    const std::vector<float>& buffer,const SpectralFrameParameters& p,
    const FrameWindowTables& tables,const RaisedCosineKernel& kernel) {
    if(!p.frame.do_fft)return reject("spectral frame requires FFT path flag");
    if(p.reference_phase || p.unsupported_branches)
        return reject("reference phase or other native branch is unsupported");
    if(buffer.size()<16 || buffer.size()>static_cast<std::size_t>(std::numeric_limits<int>::max()/4))
        return reject("invalid spectral frame buffer size");
    if(p.frame.left_length<0 || p.frame.right_length<0 ||
       static_cast<long long>(p.frame.left_length)+p.frame.right_length>static_cast<long long>(buffer.size()))
        return reject("invalid spectral frame half lengths");
    if(!std::isfinite(p.frame.ratio) || p.frame.ratio<=0 ||
       !std::isfinite(p.source_hz) || p.source_hz<=0 ||
       !std::isfinite(p.processing_sample_rate) || p.processing_sample_rate<=0 ||
       !std::isfinite(p.probe_ratio) || p.probe_ratio<0 ||
       !std::isfinite(p.output_gain) || p.output_gain<0)
        return reject("invalid spectral frame control");
    if(!p.apply_component_mask && (p.probe_ratio!=1.0f || p.output_gain!=1.0f || !p.harmonic_envelope.empty()))
        return reject("component controls requested while component mask is disabled");
    for(float x:buffer)if(!std::isfinite(x))return reject("nonfinite spectral frame input");
    const int total=p.frame.left_length+p.frame.right_length;
    int nfft=16;while(nfft<total && nfft<4096)nfft*=2;
    if(static_cast<std::size_t>(nfft)>buffer.size())return reject("buffer shorter than original selected FFT length");
    auto prepared=buffer;
    if(!initial_half(prepared,0,p.frame.left_length,tables.sqrt_hann(),0) ||
       !initial_half(prepared,p.frame.left_length,p.frame.right_length,tables.sqrt_hann(),8192))
        return reject("initial spectral window table index outside supported domain");
    // Native forward wrapper reads NFFT floats, not just L+R. No invented
    // zero-padding; initial data beyond the two halves remains caller-supplied.
    const std::vector<float> input(prepared.begin(),prepared.begin()+nfft);
    const auto forward=packed_real_forward_math(input);
    if(!forward.ok)return reject(forward.error);
    SpectralFrameResult r;r.fft_length=nfft;r.packed_spectrum=forward.values;
    const int bins=nfft/2;
    r.source_magnitude.assign(static_cast<std::size_t>(bins),0.0f);
    r.mask.assign(static_cast<std::size_t>(bins),1.0f);
    for(int bin=1;bin<bins;++bin)
        r.source_magnitude[static_cast<std::size_t>(bin)]=stable_magnitude(
            r.packed_spectrum[2*bin],r.packed_spectrum[2*bin+1]);
    if(p.apply_component_mask) {
        ComponentReconstructionParameters component;
        component.spacing_numerator=p.source_hz;
        component.spacing_denominator=p.processing_sample_rate/static_cast<float>(nfft);
        component.envelope_probe_ratio=p.probe_ratio;
        component.output_gain=p.output_gain;
        const auto mask=reconstruct_component_mask(r.source_magnitude,r.mask,component,kernel,p.harmonic_envelope);
        if(!mask.ok)return reject(mask.error);
        r.mask=mask.mask;r.target_magnitude=mask.target_magnitude;
        r.component_normalization=mask.normalization;
    }
    // Slot0/1 jointly contain DC/Nyquist. Mask0 stays1 in bypass, but becomes0
    // under this restricted component path because source/target bin0 is0.
    for(int bin=0;bin<bins;++bin) {
        r.packed_spectrum[2*bin]=r.mask[bin]*r.packed_spectrum[2*bin];
        r.packed_spectrum[2*bin+1]=r.mask[bin]*r.packed_spectrum[2*bin+1];
    }
    const auto inverse=packed_real_inverse_math(r.packed_spectrum);
    if(!inverse.ok)return reject(inverse.error);
    std::copy(inverse.values.begin(),inverse.values.end(),prepared.begin());
    // Preserve initially-windowed tail beyond NFFT, then execute only native
    // post-IFFT readout. Reapplying the initial window here would be wrong.
    const auto output=transform_frame_post_ifft({prepared},p.frame,tables);
    if(!output.ok)return reject(output.error);
    r.samples=output.channels.front();
    r.left_window_done=output.left_window_done;r.right_window_done=output.right_window_done;
    r.resample_begin=output.resample_begin;r.resample_end=output.resample_end;
    r.ok=true;return r;
}
} // namespace melodyne_re
