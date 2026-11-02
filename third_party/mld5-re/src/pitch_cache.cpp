#include "pitch_cache.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#if defined(__FAST_MATH__)
#error "Pitch cache recovery requires separately rounded arithmetic"
#endif
namespace melodyne_re {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
namespace {
float add(float a,float b){volatile float r=a+b;return r;}
float sub(float a,float b){volatile float r=a-b;return r;}
float mul(float a,float b){volatile float r=a*b;return r;}
float div(float a,float b){volatile float r=a/b;return r;}
float bit_float(std::uint32_t bits){float x;std::memcpy(&x,&bits,4);return x;}
float host_log(float x){return std::log(x);}
float host_pow(float x,float y){return std::pow(x,y);}
float host_exp(float x){return std::exp(x);}
double host_log_double(double x){return std::log(x);}
float recurrence(float state,float x,float c){return add(mul(sub(state,x),c),x);}
}
bool smooth_pitch_cache_mode1(std::vector<float>& a,float c,std::string& error,
                             double (*log_double)(double)) {
    if(a.empty())return true;
    if(!std::isfinite(c)||c<=0||c>=1){error="smoothing coefficient outside (0,1)";return false;}
    if(!log_double)log_double=host_log_double;
    // Native coefficient promoted to double before log, -2.5/log computed
    // in binary64 then truncated to signed32 (181BBE560).
    const double count=-2.5/log_double(static_cast<double>(c));
    if(!std::isfinite(count)||count>=1000000){error="smoothing padding exceeds admitted budget";return false;}
    const int extent=std::max(1,static_cast<int>(count));
    for(float x:a)if(!std::isfinite(x)){error="nonfinite smoothing input";return false;}
    const float first=a.front(),last=a.back();
    std::vector<float> right(static_cast<std::size_t>(extent),last);
    float state=first;
    // Native mode1's repeated left guard is traversed but not rewritten.
    for(int i=0;i<extent;++i)state=recurrence(state,first,c);
    for(float& x:a){state=recurrence(state,x,c);x=state;}
    for(float& x:right){state=recurrence(state,x,c);x=state;}
    // Reverse guard traversal changes state only; its forward values persist.
    for(auto i=right.rbegin();i!=right.rend();++i)state=recurrence(state,*i,c);
    for(auto i=a.rbegin();i!=a.rend();++i){state=recurrence(state,*i,c);*i=state;}
    return true;
}
PitchCacheResult build_pitch_cache_unit_factors(const std::vector<float>& periods,
    const std::vector<float>& amplitude,const PitchCacheParameters& p,const PitchCacheMath& supplied) {
    PitchCacheResult out;
    auto fail=[&](const char* s){out.error=s;return out;};
    if(periods.empty()||periods.size()>1000000||(!amplitude.empty()&&amplitude.size()!=periods.size()))
        return fail("invalid period/amplitude lengths");
    if(p.has_join||p.drift_factor!=1.f||p.modulation_factor!=1.f)
        return fail("only no-join unit drift/modulation builder admitted");
    if(!std::isfinite(p.source_sample_rate)||p.source_sample_rate<=0||
       !std::isfinite(p.control_resolution)||p.control_resolution<=0||
       !std::isfinite(p.source_duration)||p.source_duration<=0||
       !std::isfinite(p.source_pitch_center)||!std::isfinite(p.element_pitch_center))
        return fail("invalid explicit pitch-cache parameters");
    const auto logf=supplied.log_float?supplied.log_float:host_log;
    const auto powf=supplied.pow_float?supplied.pow_float:host_pow;
    const auto expf=supplied.exp_float?supplied.exp_float:host_exp;
    const auto logd=supplied.log_double?supplied.log_double:host_log_double;
    const float frequency_factor=bit_float(0x3dfa7ed2);
    const float cents_factor=bit_float(0x44d8677d);
    const float octave_factor=bit_float(0x3a5a7408); // NOT rounded float(1/1200)
    const float fs=static_cast<float>(p.source_sample_rate);
    out.source_cents.reserve(periods.size());out.display_pitch.reserve(periods.size());
    for(float value:periods){
        if(!std::isfinite(value)||value<=0)return fail("nonpositive/nonfinite source period");
        const float cents=mul(logf(mul(div(fs,value),frequency_factor)),cents_factor);
        const float pitch=add(p.element_pitch_center,sub(cents,p.source_pitch_center));
        if(!std::isfinite(pitch)||pitch<3000.f||pitch>10000.f)
            return fail("outside admitted unclamped interior pitch range");
        out.source_cents.push_back(cents);out.display_pitch.push_back(pitch);
    }
    // Native uses float(resolution)/441 then separate float multipliers4/150.
    const float scale=div(static_cast<float>(p.control_resolution),441.f);
    const float drift_arg=mul(scale,4.f),modulation_arg=mul(scale,150.f);
    auto smooth=[&](std::vector<float>& values,float arg)->bool {
        if(arg<std::numeric_limits<float>::min())return true;
        const float coefficient=expf(div(-2.5f,arg));
        if(coefficient<std::numeric_limits<float>::min())return true;
        return smooth_pitch_cache_mode1(values,coefficient,out.error,logd);
    };
    out.drift_smooth=out.display_pitch;
    if(!smooth(out.drift_smooth,drift_arg))return out;
    for(std::size_t i=0;i<periods.size();++i){
        const float a=add(mul(sub(out.drift_smooth[i],p.element_pitch_center),p.drift_factor),p.element_pitch_center);
        out.display_pitch[i]=add(a,sub(out.display_pitch[i],out.drift_smooth[i]));
    }
    out.modulation_smooth=out.display_pitch;
    const float width=std::fmin(mul(static_cast<float>(p.source_duration),.5f),.1f);
    const double raw_count=static_cast<double>(width)*p.control_resolution;
    if(!std::isfinite(raw_count)||raw_count<0||raw_count>static_cast<double>(periods.size()/2))
        return fail("endpoint taper exceeds admitted nonoverlapping array support");
    const int taper_count=static_cast<int>(raw_count);
    const float inverse_count=div(1.f,static_cast<float>(taper_count));
    for(int j=0;j<taper_count;++j){
        const float fraction=mul(static_cast<float>(j),inverse_count);
        const float common=mul(sub(1.f,fraction),p.element_pitch_center);
        const auto lo=static_cast<std::size_t>(j),hi=periods.size()-1-lo;
        out.modulation_smooth[lo]=add(common,mul(fraction,out.modulation_smooth[lo]));
        out.modulation_smooth[hi]=add(common,mul(fraction,out.modulation_smooth[hi]));
    }
    if(!amplitude.empty())for(std::size_t i=0;i<periods.size();++i){
        if(!std::isfinite(amplitude[i]))return fail("nonfinite amplitude envelope");
        if(amplitude[i]<.000001f)out.modulation_smooth[i]=p.element_pitch_center;
    }
    if(!smooth(out.modulation_smooth,modulation_arg))return out;
    out.playback_ratio.reserve(periods.size());
    for(std::size_t i=0;i<periods.size();++i){
        const float a=add(mul(sub(out.modulation_smooth[i],p.element_pitch_center),p.modulation_factor),p.element_pitch_center);
        out.display_pitch[i]=add(a,sub(out.display_pitch[i],out.modulation_smooth[i]));
        if(!std::isfinite(out.display_pitch[i])||out.display_pitch[i]<3000.f||out.display_pitch[i]>10000.f)
            return fail("processed pitch outside admitted unclamped interior");
        // Original recomputes logf from original period, rather than using its
        // first cents temporary. Preserve that second call for explicit math.
        const float cents=mul(logf(mul(div(fs,periods[i]),frequency_factor)),cents_factor);
        const float ratio=powf(2.f,mul(sub(out.display_pitch[i],cents),octave_factor));
        if(!std::isfinite(ratio)||ratio<=0)return fail("invalid generated playback ratio");
        out.playback_ratio.push_back(ratio);
    }
    out.resolution=p.control_resolution;out.ok=true;return out;
}
} // namespace melodyne_re
