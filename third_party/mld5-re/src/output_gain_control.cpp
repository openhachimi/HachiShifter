#include "output_gain_control.hpp"
#include <cmath>
#include <cstring>
#include <stdexcept>
#if defined(__FAST_MATH__)
#error "Output-gain control requires separately rounded arithmetic"
#endif
namespace melodyne_re {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
namespace {
double product(double a,double b) noexcept {volatile double r=a*b;return r;}
double quotient(double a,double b) noexcept {volatile double r=a/b;return r;}
float float_negate(float x) noexcept {std::uint32_t b;std::memcpy(&b,&x,4);b^=0x80000000u;std::memcpy(&x,&b,4);return x;}
}
OutputGainThreshold calculate_component_shutdown_threshold(double fs,double end,double ds,
    bool source_feature,bool period) noexcept {
    if(!std::isfinite(fs)||fs<=0||!std::isfinite(end)||!std::isfinite(ds))
        return {false,0,"requires finite endpoint/step and positive source rate"};
    volatile double difference=ds-1.0;
    if(!source_feature && !(std::fabs(difference)>1e-6 && period))
        return {true,std::numeric_limits<double>::max(),nullptr};
    const double extent=product(fs,.0025);
    if(!std::isfinite(extent)||extent>=9223372036854775808.0)
        return {false,0,"native signed64 threshold conversion hazard"};
    const auto frames=static_cast<std::int64_t>(extent);
    volatile double threshold=end-static_cast<double>(frames);
    if(!std::isfinite(threshold))return {false,0,"nonfinite threshold subtraction"};
    return {true,threshold,nullptr};
}
OutputGainThreshold refresh_component_shutdown_threshold(OutputGainState& s,double fs,double end,
    double ds,bool feature,bool period) noexcept {
    if(s.threshold==std::numeric_limits<double>::max())return {true,s.threshold,nullptr};
    auto result=calculate_component_shutdown_threshold(fs,end,ds,feature,period);
    if(result.ok)s.threshold=result.value;
    return result;
}
OutputGainTransition component_shutdown_after_advance(OutputGainState& s,double source,double ds,
    double fs,bool inhibit,bool empty) noexcept {
    OutputGainTransition result;
    if(!std::isfinite(source)||!std::isfinite(s.threshold)||!std::isfinite(s.value)||!std::isfinite(s.delta)){
        result.error="nonfinite state/source coordinate";return result;
    }
    if(!inhibit && s.threshold>=0 && source>=s.threshold){
        if(empty){
            if(!std::isfinite(fs)||fs<=0||!std::isfinite(ds)||ds==0){result.error="invalid fade sample-rate/step";return result;}
            const double count=quotient(product(fs,.0025),ds);
            // Native CVTTSD2SI followed by abs32. Exclude INT_MIN, zero divisor
            // and out-of-range conversions rather than invoking C++ UB/Inf.
            if(!std::isfinite(count)||count<=-2147483648.0||count>=2147483648.0){result.error="native signed32 fade conversion hazard";return result;}
            const auto signed_count=static_cast<std::int32_t>(count);
            const auto n=signed_count<0?-signed_count:signed_count;
            if(n==0){result.error="native fade divisor is zero";return result;}
            const float denominator=static_cast<float>(n);
            volatile float ratio=s.value/denominator;
            s.delta=float_negate(ratio);
            s.gain_remaining=s.source_remaining=std::numeric_limits<std::int32_t>::max();
            s.fading=true;
            result.shutdown_started=result.freeze_source_refresh=true;
            result.nominal_fade_samples=n;
        }
        // Queue nonempty STILL consumes the threshold, without starting fade.
        s.threshold=std::numeric_limits<double>::max();result.threshold_consumed=true;
    }
    result.ok=true;
    // Original next finite-gain stop predicate; virtual stop implementation is
    // caller-owned and is not executed/reconstructed by this primitive.
    result.request_stop=s.fading && s.value<=0.0f;
    return result;
}
OutputGainValue multiply_output_gain(float sample,const OutputGainState& s) noexcept {
    if(!std::isfinite(sample)||!std::isfinite(s.value))return {false,0,"nonfinite plain gain multiply"};
    volatile float value=s.value*sample;
    if(!std::isfinite(value))return {false,0,"plain gain multiply overflow"};
    return {true,value,nullptr};
}
OutputGainValue advance_output_gain_value(OutputGainState& s) noexcept {
    if(!std::isfinite(s.value)||!std::isfinite(s.delta))return {false,0,"nonfinite gain advance"};
    volatile float value=s.delta+s.value;
    if(!std::isfinite(value))return {false,0,"gain advance overflow"};
    s.value=value;return {true,value,nullptr};
}
namespace {
struct GainRenderBridge {
    OutputGainRenderParameters parameters;
    double fs=0;
    OutputGainState state;
    OutputGainRenderRecords* records=nullptr;
    void (*previous_observer)(std::size_t,const PeriodClockState&,void*)=nullptr;
    void* previous_context=nullptr;
    bool initialized=false,stopped=false;
    static void before(std::size_t n,const PeriodClockState& clock,void* raw){
        auto& b=*static_cast<GainRenderBridge*>(raw);
        if(!b.initialized){b.state.threshold=0;b.state.gain_remaining=std::numeric_limits<std::int32_t>::max();b.initialized=true;}
        const auto threshold=refresh_component_shutdown_threshold(b.state,b.fs,b.parameters.signal_end,clock.source_step,b.parameters.source_feature,true);
        if(!threshold.ok)throw std::runtime_error(threshold.error);
        if(n==0)b.records->initial_threshold=threshold.value;
        if(b.stopped&&!b.parameters.last_component_zero_remainder)throw std::runtime_error("last-component zero remainder not admitted");
        b.records->emitted_gain.at(n)=b.stopped?0.0f:b.state.value;
        if(b.previous_observer)b.previous_observer(n,clock,b.previous_context);
    }
    static SourceTimeAfterOutputAction after(std::size_t n,const SourceTimeControlState& source,const PeriodClockState&,void* raw){
        auto& b=*static_cast<GainRenderBridge*>(raw);
        if(b.stopped)return SourceTimeAfterOutputAction::keep;
        const auto advanced=advance_output_gain_value(b.state);
        if(!advanced.ok)throw std::runtime_error(advanced.error);
        b.state.source_remaining=source.remaining;
        const auto transition=component_shutdown_after_advance(b.state,source.source_samples,source.source_step,b.fs,b.parameters.inhibit_322,b.parameters.pending_queue_empty);
        if(!transition.ok)throw std::runtime_error(transition.error);
        if(transition.shutdown_started&&!b.records->shutdown_started){
            b.records->shutdown_started=true;b.records->trigger_after_output=n;
            b.records->trigger_source_after=source.source_samples;b.records->trigger_source_step=source.source_step;
            b.records->fade_divisor=transition.nominal_fade_samples;
        }
        if(transition.request_stop){
            if(!b.parameters.last_component_zero_remainder&&n+1<b.records->emitted_gain.size())throw std::runtime_error("stop before block end requires explicit last-component zero policy");
            b.stopped=true;b.records->stop_requested=true;b.records->stop_after_output=n;
        }
        return transition.freeze_source_refresh?SourceTimeAfterOutputAction::freeze_source_refresh:SourceTimeAfterOutputAction::keep;
    }
};
}
OutputGainRenderResult render_period_component_source_time_gain(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const SampledFunctionView& playback,IdentitySourceTimeView source_time,
    const PeriodRendererParameters& parameters,const SourceTimeRenderContext& caller,const OutputGainRenderParameters& gain){
    OutputGainRenderResult out;
    if(!std::isfinite(gain.signal_end)||gain.signal_end<=0){out.error="explicit positive finite source signal end required";return out;}
    if(parameters.clip_output){out.error="post-recorded plain gain requires clipfalse";return out;}
    if(caller.after_output_action){out.error="output gain requires sole ownership of after-output action";return out;}
    if(parameters.output_count==0||parameters.output_count>100000000||
       caller.time.output_block_count<1||parameters.output_count!=static_cast<std::size_t>(caller.time.output_block_count)){
        out.error="output gain requires bounded matching output block";return out;
    }
    out.records.emitted_gain.resize(parameters.output_count);
    GainRenderBridge bridge;bridge.parameters=gain;bridge.fs=parameters.source_sample_rate;bridge.records=&out.records;
    bridge.previous_observer=caller.observe_before_output;bridge.previous_context=caller.observation_context;
    auto controlled=caller;controlled.observe_before_output=GainRenderBridge::before;controlled.observation_context=&bridge;
    controlled.after_output_action=GainRenderBridge::after;controlled.after_output_context=&bridge;
    try {
        out.raw=render_period_component_source_time(pcm,periods,playback,source_time,parameters,controlled);
        out.records.final_gain=bridge.state;
        if(!out.raw.ok){out.error=out.raw.error;return out;}
        if(out.raw.renderer.samples.size()!=out.records.emitted_gain.size()){out.error="gain/raw output length mismatch";return out;}
        out.samples=out.raw.renderer.samples;
        OutputGainState multiplier;
        for(std::size_t n=0;n<out.samples.size();++n){
            multiplier.value=out.records.emitted_gain[n];const auto value=multiply_output_gain(out.samples[n],multiplier);
            if(!value.ok){out.error=value.error;return out;}out.samples[n]=value.value;
        }
    }catch(const std::exception& error){out.error=error.what();return out;}
    out.ok=true;return out;
}
} // namespace melodyne_re
