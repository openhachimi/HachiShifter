#include "cached_control.hpp"
#include "period_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#if defined(__FAST_MATH__)
#error "Recovered control arithmetic requires no fast-math"
#endif
namespace melodyne_re {
namespace {
double mul(double a,double b){volatile double r=a*b;return r;}
double sub(double a,double b){volatile double r=a-b;return r;}
double div(double a,double b){volatile double r=a/b;return r;}
double add(double a,double b){volatile double r=a+b;return r;}
float mulf(float a,float b){volatile float r=a*b;return r;}
bool parameters_ok(const CachedRateParameters& p){
    return std::isfinite(p.source_sample_rate)&&p.source_sample_rate>0&&
        std::isfinite(p.output_sample_rate)&&p.output_sample_rate>0&&
        p.source_stride==1&&p.global_pitch==1.f;
}
double base_rate(const CachedRateParameters& p){
    return mul(div(p.source_sample_rate,static_cast<double>(static_cast<float>(p.source_stride))),
               div(1.0,p.output_sample_rate));
}
}
bool initialize_cached_rate(const SampledFunctionView& view,double x,
    const CachedRateParameters& p,CachedRateState& s,const char*& error) noexcept {
    error=nullptr;
    if(!parameters_ok(p)||!view.values||view.count<2||!std::isfinite(x)){
        error="cached rate requires explicit sampled data, finite x, identity D1/global1 parameters";return false;
    }
    auto point=evaluate_sampled_function(view,x,false);
    if(!point.ok){error=point.error;return false;}
    const float product=mulf(static_cast<float>(point.value),p.global_pitch);
    const double rate=mul(base_rate(p),static_cast<double>(product));
    if(!std::isfinite(rate)||rate<=0){error="invalid initial cached pitch/rate";return false;}
    CachedRateState next=s;next.initialized=true;next.rate=rate;next.delta=0;next.initial_pitch=product;next.remaining=0;next.refreshes=0;
    s=next;return true;
}
CachedRateRefresh refresh_cached_rate(const SampledFunctionView& view,double x,double dx,
    int requested_count,const CachedRateParameters& p,CachedRateState& s) noexcept {
    CachedRateRefresh out;
    auto fail=[&](const char* why){out.error=why;return out;};
    if(!s.initialized||s.remaining!=0||!parameters_ok(p)||!view.values||view.count<2||requested_count<1||
       !std::isfinite(s.rate)||s.rate<=0)
        return fail("invalid cached-rate refresh state/request");
    const auto segment=evaluate_sampled_segment_forward(view,x,dx,requested_count,s.cached_index,false);
    if(!segment.ok)return fail(segment.error);
    if(segment.count<1)return fail("native segment returned nonpositive count");
    const float product=mulf(static_cast<float>(segment.value),p.global_pitch);
    const double endpoint_rate=mul(base_rate(p),static_cast<double>(product));
    const double delta=div(sub(endpoint_rate,s.rate),static_cast<double>(static_cast<float>(segment.count)));
    if(!std::isfinite(endpoint_rate)||endpoint_rate<=0||!std::isfinite(delta)||s.refreshes==std::numeric_limits<std::size_t>::max())
        return fail("invalid cached endpoint/rate delta");
    s.delta=delta;s.remaining=segment.count;s.cached_index=segment.cached_index;++s.refreshes;
    out.ok=true;out.accessor_endpoint=segment.value;out.endpoint_pitch=product;out.endpoint_rate=endpoint_rate;
    out.returned_count=segment.count;out.returned_index=segment.cached_index;return out;
}
bool advance_cached_rate(CachedRateState& s,const char*& error) noexcept {
    error=nullptr;
    if(!s.initialized||s.remaining<1){error="cached rate output consumed without a current segment";return false;}
    const double rate=add(s.rate,s.delta);
    if(!std::isfinite(rate)||rate<=0){error="cached rate advance became invalid";return false;}
    s.rate=rate;--s.remaining;return true;
}
namespace {
class RendererCachedRate final : public detail::PeriodRendererRateControl {
    SampledFunctionView playback_;
    CachedRateParameters parameters_;
    int request_budget_;
public:
    CachedRateState state;
    std::vector<PeriodRateControlEvent> events;
    RendererCachedRate(SampledFunctionView playback,const PeriodRendererParameters& p,int budget)
      :playback_(playback),request_budget_(budget) {
        parameters_.source_sample_rate=p.source_sample_rate;
        parameters_.output_sample_rate=p.output_sample_rate;
    }
    bool initialize(PeriodStartupParameters& startup,const char*& error) override {
        const double inverse_fs=div(1.0,parameters_.source_sample_rate);
        if(!initialize_cached_rate(playback_,mul(startup.source_position,inverse_fs),parameters_,state,error))return false;
        startup.rate=state.rate;startup.rate_delta=state.delta;
        startup.target_pitch_ratio=state.initial_pitch;return true;
    }
    bool before_output(PeriodClockState& clock,std::size_t output_index,
                       std::size_t remaining_outputs,const char*& error) override {
        error=nullptr;
        if(state.remaining==0) {
            const int budget=static_cast<int>(std::min(remaining_outputs,static_cast<std::size_t>(request_budget_)));
            const double inverse_fs=div(1.0,parameters_.source_sample_rate);
            const double x=mul(clock.source_position,inverse_fs),dx=mul(clock.source_step,inverse_fs);
            const double current=state.rate;
            const auto endpoint=refresh_cached_rate(playback_,x,dx,budget,parameters_,state);
            if(!endpoint.ok){error=endpoint.error;return false;}
            PeriodRateControlEvent e;e.output_index=output_index;e.source_seconds=x;e.source_step_seconds=dx;
            e.requested_count=budget;e.returned_count=endpoint.returned_count;e.cached_index=endpoint.returned_index;
            e.accessor_endpoint=endpoint.accessor_endpoint;e.endpoint_pitch=endpoint.endpoint_pitch;
            e.current_rate=current;e.endpoint_rate=endpoint.endpoint_rate;e.delta=state.delta;events.push_back(e);
        }
        clock.rate=state.rate;clock.rate_delta=state.delta;return true;
    }
    bool after_output(const PeriodClockState& clock,const char*& error) override {
        if(!advance_cached_rate(state,error))return false;
        if(state.rate!=clock.rate){error="cached controller and period clock advanced R differently";return false;}
        return true;
    }
};
}
PeriodRendererResult render_period_component_cached(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const SampledFunctionView& playback,
    const PeriodRendererParameters& p,int requested_count_budget) {
    if(requested_count_budget<1||requested_count_budget>100000000) {
        PeriodRendererResult r;r.error="cached renderer requires explicit budget within1..100000000";return r;
    }
    RendererCachedRate controller(playback,p,requested_count_budget);
    auto parameters=p;parameters.pitch_ratio=1.0; // cached initialization supplies actual R
    auto result=detail::render_period_component_controlled(pcm,periods,parameters,&controller);
    result.rate_controls=std::move(controller.events);result.final_control_count=controller.state.remaining;
    return result;
}
} // namespace melodyne_re
