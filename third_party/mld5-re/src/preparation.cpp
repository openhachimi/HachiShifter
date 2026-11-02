#include "preparation.hpp"
#include <cmath>
#include <limits>
#include <cstring>

#if defined(__FAST_MATH__)
#error "Recovered preparation arithmetic must not use fast-math"
#endif
namespace melodyne_re {
PreparationEnvelopeResult evaluate_preparation_envelope(
    const PreparationEnvelopeSnapshot& e,float seconds) {
    PreparationEnvelopeResult r;
    const float fields[]={e.start,e.rise_duration,e.rise_end,e.decay_start,e.decay_factor,e.end,seconds};
    for(float x:fields)if(!std::isfinite(x) || x<0.0f){
        r.error="envelope snapshot requires finite nonnegative inputs";return r;
    }
    constexpr float tiny=std::numeric_limits<float>::min();
    if(e.rise_duration>seconds) {
        r.value=e.start;
        if(e.rise_end!=e.start && e.rise_duration>=tiny) {
            const float ratio=seconds/e.rise_duration;
            const float power=std::pow(ratio,2.0f);
            r.value=power*(e.rise_end-e.start)+e.start;
        }
    } else if(e.decay_start>seconds || e.end==e.rise_end) {
        r.value=e.rise_end;
    } else {
        const float delta=seconds-e.decay_start;
        float decay=1.0f;
        if(delta>=tiny)decay=std::pow(e.decay_factor,delta*1000.0f);
        const float tail=(e.rise_end-e.end)*decay;
        r.value=std::fmax(0.0f,tail+e.end);
        if(e.end>e.rise_end)r.value=std::fmin(e.end*1.1f,r.value);
    }
    if(!std::isfinite(r.value)){r.error="envelope arithmetic overflow";return r;}
    r.ok=true;return r;
}

PreparationSpectrumResult derive_neutral_harmonic_factors(
    const PreparationNeutralSpectrum& s) {
    PreparationSpectrumResult r;
    if(s.spectrum_type<0 || s.spectrum_type>2 ||
       s.one_by_f_tilt_pow!=1.0f || s.one_by_f_diff_pow!=0.0f ||
       s.comb_start!=1.0f || s.comb_balance!=0.0f || s.comb_width!=2.0f) {
        r.error="unsupported nonneutral or equalizer spectrum";return r;
    }
    if(s.base_target.size()!=512 || s.reference.size()!=512 ||
       s.unreduced_factors.size()!=512 || s.formant_factors.size()!=512) {
        r.error="neutral harmonic arrays must have512 entries";return r;
    }
    if(std::memcmp(s.base_target.data(),s.reference.data(),512*sizeof(float))!=0) {
        r.error="base/reference data are not bit-identical";return r;
    }
    for(std::size_t i=0;i<512;++i) {
        if(!std::isfinite(s.reference[i]) || s.reference[i]<0.0000001f ||
           s.unreduced_factors[i]!=1.0f || s.formant_factors[i]!=1.0f) {
            r.error="unsupported neutral spectrum data domain";return r;
        }
    }
    r.applied_factors.resize(512);
    // Retain the original powf call, rather than let constant-exponent
    // optimization substitute division. The host CRT remains an explicit
    // difference from unverified original Windows CRT rounding.
    float (*volatile power)(float,float)=::powf;
    for(int bin=0;bin<512;++bin) {
        // Neutral comb updater fast path leaves every comb factor exactly1.
        const float target=std::fmax(0.0000001f,s.base_target[bin]);
        float value=target/s.reference[bin];
        if(bin>0) {
            value=power(value,1.0f); // neutral difference exponent
            const float correction=power(power(1.0f,1.0f),-0.0f);
            value=value*correction;
            const float h=static_cast<float>(bin);
            const float tilt=power(h,-1.0f)*h;
            value=value*tilt;
            value=std::fmin(20000.0f,value);
        }
        value=value*1.0f; // comb factor
        value=value*s.unreduced_factors[bin];
        value=std::fmax(0.0000001f,value);
        r.applied_factors[bin]=value;
        if(value<0.999f || value>1.001f)r.needs_process_spectrum=true;
    }
    r.ok=true;return r;
}

bool preparation_envelope_needs_process(const PreparationEnvelopeSnapshot& e,
                                        bool bypassed) noexcept {
    return !bypassed && (e.start!=1.0f || e.rise_end!=1.0f || e.end!=1.0f);
}
bool preparation_formants_need_process(const std::vector<float>& factors) noexcept {
    for(std::size_t i=1;i<factors.size();++i)
        if(!(factors[i]>=0.999f && factors[i]<=1.001f))return true;
    return false;
}
PreparationSetPolicy derive_preparation_set_policy(const PreparationSetPolicyInput& p) noexcept {
    PreparationSetPolicy r;
    if(!p.can_use || p.is_bypassed)return r;
    r.process_lower_spectrum=p.lower_spectrum;
    r.process_upper_spectrum=p.upper_spectrum;
    r.process_spectrum_shaper=p.common_spectrum || p.lower_spectrum || p.upper_spectrum ||
        p.common_formant || p.lower_formant || p.upper_formant || p.equalizer_formant ||
        p.equalizer || p.amplitude_envelope || p.difference_envelope || p.formant_envelope ||
        p.synth_amplitudes!=0.0f || p.synth_phases!=0.0f ||
        p.amplitude_difference_power!=1.0f || p.emphasize_power!=0.0f;
    return r;
}
} // namespace melodyne_re
