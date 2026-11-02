#include "source_time_control.hpp"
#include "cached_control.hpp"
#include <algorithm>
#include <utility>
#include <cmath>
#include <limits>
#if defined(__FAST_MATH__)
#error "Source time recovery requires separate binary64 rounding"
#endif
namespace melodyne_re {
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
namespace {
double add(double a,double b){volatile double r=a+b;return r;}
double sub(double a,double b){volatile double r=a-b;return r;}
double mul(double a,double b){volatile double r=a*b;return r;}
double div(double a,double b){volatile double r=a/b;return r;}
bool curve_ok(IdentitySourceTimeView v){
 if(!v.knots||v.count<2||v.count>1000000||v.knots[0]!=0)return false;
 for(std::size_t i=0;i<v.count;++i)if(!std::isfinite(v.knots[i])||(i&&!(v.knots[i]>v.knots[i-1])))return false;
 return true;
}
SourceTimeAccess fail(int n,int index,const char* why){return {false,0,n,index,why};}
SourceTimeAccess done(double value,int n,int index){
 if(!std::isfinite(value))return fail(n,index,"nonfinite source-time result");
 return {true,value,n,index,nullptr};
}
// Native cache lookup tries at most3 adjacent intervals. Bounded sorted-search
// fallback returns the enclosing interval, including previous interval atlast.
int interval(IdentitySourceTimeView v,double x,int cache){
 const int last=static_cast<int>(v.count)-1;
 if(cache>=0&&cache<=last){
  if(x<v.knots[cache]){
   for(int lo=cache-1,tries=0;lo>=0&&tries<3;--lo,++tries)
    if(x>=v.knots[lo]&&x<v.knots[lo+1])return lo;
  }else if(cache<last){
   for(int lo=cache,tries=0;lo<last&&tries<3;++lo,++tries)
    if(x>=v.knots[lo]&&x<v.knots[lo+1])return lo;
  }
 }
 int lo=0,hi=last;
 while(lo+1<hi){const int mid=lo+(hi-lo)/2;if(v.knots[mid]<x)lo=mid;else hi=mid;}
 return lo;
}
double line(IdentitySourceTimeView v,int lo,int hi,double x){
 const double dy=sub(v.knots[hi],v.knots[lo]),dx=sub(v.knots[hi],v.knots[lo]);
 return add(mul(div(dy,dx),sub(x,v.knots[lo])),v.knots[lo]);
}
bool parameters_ok(const SourceTimeControlParameters&p){
 return std::isfinite(p.source_sample_rate)&&p.source_sample_rate>0&&
   p.component_source_offset>=-9007199254740992LL&&p.component_source_offset<=9007199254740992LL;
}
}
SourceTimeAccess evaluate_identity_source_point(IdentitySourceTimeView v,double x,int cache,bool extrapolate) noexcept {
 if(!curve_ok(v)||!std::isfinite(x))return fail(0,cache,"invalid identity source curve/query");
 const int last=static_cast<int>(v.count)-1;
 int output_index=0;
 if(x<=0){if(!extrapolate)return done(0,0,0);cache=0;output_index=0;}
 else if(x>v.knots[last]){if(!extrapolate)return done(v.knots[last],0,last);cache=last-1;output_index=last;}
 else {cache=interval(v,x,cache);output_index=cache;}
 const double lx=v.knots[cache],rx=v.knots[cache+1];
 // Point accessor uses nearest-anchor multiply-then-divide, not segment's slope.
 const double dy=sub(rx,lx),dx=sub(rx,lx);
 if(mul(add(rx,lx),.5)<x)return done(add(div(mul(sub(x,rx),dy),dx),rx),0,output_index);
 return done(add(div(mul(sub(x,lx),dy),dx),lx),0,output_index);
}
SourceTimeAccess evaluate_identity_source_segment(IdentitySourceTimeView v,double x,double dx,
 int requested,int cache,bool extrapolate) noexcept {
 if(!curve_ok(v)||!std::isfinite(x)||!std::isfinite(dx)||dx<=0)
  return fail(requested,cache,"invalid identity source segment domain");
 const int last=static_cast<int>(v.count)-1;int count=requested<1?1:requested;
 double end=add(mul(static_cast<double>(count),dx),x);
 if(!std::isfinite(end))return fail(requested,cache,"nonfinite source segment extent");
 if(x<0){
  bool crossed=false;
  if(end>=0){const double q=div(sub(0,x),dx);if(q>=2147483647.)return fail(requested,cache,"left count hazard");count=static_cast<int>(q)+1;crossed=true;}
  if(extrapolate||crossed)return done(line(v,0,1,add(mul(static_cast<double>(count),dx),x)),count,0);
  return done(0,count,0);
 }
 if(x>v.knots[last])return done(extrapolate?line(v,last-1,last,add(x,dx)):v.knots[last],1,last);
 int lo=interval(v,x,cache),hi=lo+1,crossed_one=0;
 const double first=add(x,dx);
 if(!std::isfinite(first))return fail(requested,cache,"nonfinite first source step");
 if(first>v.knots[hi]){
  while(true){lo=hi;++hi;if(hi>last)return done(extrapolate?line(v,last-1,last,first):v.knots[last],1,last);
   if(first<=v.knots[hi]){crossed_one=1;break;}}
 }
 if(end>v.knots[hi]){
  const double q=div(sub(v.knots[hi],x),dx);
  if(!std::isfinite(q)||q<0||q>=2147483647.)return fail(requested,cache,"forward count hazard");
  int steps=static_cast<int>(q);
  if(hi==last&&extrapolate)++steps;
  if(steps<1)steps=1;
  if(crossed_one&&steps==std::numeric_limits<int>::max())return fail(requested,cache,"count increment hazard");
  count=steps+crossed_one;
  end=add(mul(static_cast<double>(steps),dx),x); // not count*dx when crossingflag1
 }
 return done(line(v,lo,hi,end),count,lo);
}
SourceTimeContextResult derive_source_time_context(const SourceTimeContext& c) noexcept {
 SourceTimeContextResult out;
 if(c.output_block_count<1||!std::isfinite(c.begin_time)||!std::isfinite(c.end_time)||
    !std::isfinite(c.element_start)||(c.previous_element_start_valid&&!std::isfinite(c.previous_element_start))){
  out.error="invalid explicit source-time block context";return out;
 }
 const double inverse_count=div(1.,static_cast<double>(c.output_block_count));
 const double interval_time=sub(c.end_time,c.begin_time);
 out.element_relative_begin=sub(c.begin_time,c.element_start);
 out.output_time_step=mul(interval_time,inverse_count);
 out.accessor_time_step=out.output_time_step;
 if(c.previous_element_start_valid)
  out.accessor_time_step=mul(sub(interval_time,sub(c.element_start,c.previous_element_start)),inverse_count);
 if(!std::isfinite(out.element_relative_begin)||!std::isfinite(out.output_time_step)||out.output_time_step<=0||
    !std::isfinite(out.accessor_time_step)||out.accessor_time_step<=0){out.error="only finite forward source-time contexts admitted";return out;}
 out.ok=true;return out;
}
bool initialize_source_time_control(IdentitySourceTimeView curve,double x,const SourceTimeControlParameters&p,
 SourceTimeControlState&s,const char*&error) noexcept {
 error=nullptr;if(!parameters_ok(p)){error="invalid explicit source scalar parameters";return false;}
 const auto initial=evaluate_identity_source_point(curve,x,s.cached_index,true);
 if(!initial.ok){error=initial.error;return false;}
 const double samples=add(mul(initial.value,p.source_sample_rate),static_cast<double>(p.component_source_offset));
 if(!std::isfinite(samples)){error="nonfinite initial source sample position";return false;}
 s.initialized=true;s.source_samples=samples;s.source_step=0;s.remaining=0;s.cached_index=initial.cached_index;return true;
}
SourceTimeAccess refresh_source_time_control(IdentitySourceTimeView curve,double x,double dx,int budget,
 const SourceTimeControlParameters&p,SourceTimeControlState&s) noexcept {
 if(!parameters_ok(p)||!s.initialized||s.remaining!=0||budget<1||!std::isfinite(s.source_samples))
  return fail(budget,s.cached_index,"invalid source control refresh state/budget");
 const auto endpoint=evaluate_identity_source_segment(curve,x,dx,budget,s.cached_index,true);
 if(!endpoint.ok)return endpoint;
 const double target=add(mul(endpoint.value,p.source_sample_rate),static_cast<double>(p.component_source_offset));
 const double delta=div(sub(target,s.source_samples),static_cast<double>(endpoint.count)); // integer->DOUBLE
 if(!std::isfinite(delta)||delta<=0)return fail(budget,s.cached_index,"nonpositive/nonfinite source step");
 s.source_step=delta;s.remaining=endpoint.count;s.cached_index=endpoint.cached_index;return endpoint;
}
bool advance_source_time_control(SourceTimeControlState&s,const char*&error) noexcept {
 error=nullptr;if(!s.initialized||s.remaining<1){error="source control consumed without current segment";return false;}
 const double samples=add(s.source_samples,s.source_step);
 if(!std::isfinite(samples)){error="source position overflow";return false;}
 s.source_samples=samples;--s.remaining;return true;
}
namespace {
class ExplicitSourceRateControl final : public detail::PeriodRendererRateControl {
    IdentitySourceTimeView source_curve_;
    SampledFunctionView playback_;
    SourceTimeRenderContext caller_;
    SourceTimeContextResult context_;
    SourceTimeControlParameters source_parameters_;
    CachedRateParameters rate_parameters_;
    std::size_t current_output_index_=0;
public:
    SourceTimeControlState source;
    CachedRateState rate;
    double time;
    std::vector<SourceTimeControlEvent> source_events;
    std::vector<PeriodRateControlEvent> rate_events;
    ExplicitSourceRateControl(IdentitySourceTimeView c,SampledFunctionView r,
      const PeriodRendererParameters& p,const SourceTimeRenderContext& caller)
      :source_curve_(c),playback_(r),caller_(caller),context_(derive_source_time_context(caller.time)),time(caller.time.begin_time) {
        source_parameters_.source_sample_rate=p.source_sample_rate;
        rate_parameters_.source_sample_rate=p.source_sample_rate;
        rate_parameters_.output_sample_rate=p.output_sample_rate;
    }
    bool refresh_source(std::size_t output_index,bool bootstrap,const char*& error) {
        const double before=source.source_samples;
        const double x=sub(time,caller_.time.element_start);
        const auto e=refresh_source_time_control(source_curve_,x,context_.accessor_time_step,
            caller_.source_refresh_budget,source_parameters_,source);
        if(!e.ok){error=e.error;return false;}
        SourceTimeControlEvent event;event.output_index=output_index;event.bootstrap=bootstrap;
        event.element_time=x;event.source_before=before;event.endpoint=e.value;
        event.returned_count=e.count;event.cached_index=e.cached_index;event.source_step=source.source_step;
        source_events.push_back(event);return true;
    }
    bool initialize(PeriodStartupParameters& startup,const char*& error) override {
        if(!context_.ok){error=context_.error;return false;}
        if(!initialize_source_time_control(source_curve_,context_.element_relative_begin,source_parameters_,source,error))return false;
        // Explicitly admitted one-query bootstrap supplies positive dS to startup
        // without consuming the first segment. Do not generalize to all preludes.
        if(!refresh_source(0,true,error))return false;
        if(!initialize_cached_rate(playback_,mul(source.source_samples,div(1.,source_parameters_.source_sample_rate)),
                                   rate_parameters_,rate,error))return false;
        startup.source_position=source.source_samples;startup.source_step=source.source_step;
        startup.rate=rate.rate;startup.rate_delta=rate.delta;startup.target_pitch_ratio=rate.initial_pitch;return true;
    }
    bool before_output(PeriodClockState& clock,std::size_t output_index,std::size_t,const char*& error) override {
        if(source.remaining==0&&!refresh_source(output_index,false,error))return false;
        if(rate.remaining==0){
            const double inverse_fs=div(1.,source_parameters_.source_sample_rate);
            const double x=mul(source.source_samples,inverse_fs),dx=mul(source.source_step,inverse_fs);
            const int budget=std::min(source.remaining,caller_.rate_external_budget);
            const double current=rate.rate;
            const auto e=refresh_cached_rate(playback_,x,dx,budget,rate_parameters_,rate);
            if(!e.ok){error=e.error;return false;}
            PeriodRateControlEvent event;event.output_index=output_index;event.source_seconds=x;event.source_step_seconds=dx;
            event.requested_count=budget;event.returned_count=e.returned_count;event.cached_index=e.returned_index;
            event.accessor_endpoint=e.accessor_endpoint;event.endpoint_pitch=e.endpoint_pitch;event.current_rate=current;
            event.endpoint_rate=e.endpoint_rate;event.delta=rate.delta;rate_events.push_back(event);
        }
        clock.source_position=source.source_samples;clock.source_step=source.source_step;
        clock.rate=rate.rate;clock.rate_delta=rate.delta;
        current_output_index_=output_index;
        if(caller_.observe_before_output)caller_.observe_before_output(output_index,clock,caller_.observation_context);
        return true;
    }
    bool after_output(const PeriodClockState& clock,const char*& error) override {
        if(!advance_source_time_control(source,error)||!advance_cached_rate(rate,error))return false;
        if(clock.source_position!=source.source_samples||clock.rate!=rate.rate){error="source/R controller and renderer clocks differ";return false;}
        time=add(time,context_.output_time_step);
        if(caller_.after_output_action){
            const auto action=caller_.after_output_action(current_output_index_,source,clock,caller_.after_output_context);
            if(action==SourceTimeAfterOutputAction::freeze_source_refresh)source.remaining=std::numeric_limits<int>::max();
            else if(action!=SourceTimeAfterOutputAction::keep){error="unsupported after-output source action";return false;}
        }
        return true;
    }
};
}
SourceTimeRenderResult render_period_component_source_time(const std::vector<float>& pcm,
 const SampledFunctionView& periods,const SampledFunctionView& playback,IdentitySourceTimeView source_time,
 const PeriodRendererParameters& p,const SourceTimeRenderContext& caller) {
    SourceTimeRenderResult out;
    if(!caller.admit_pre_startup_source_refresh){out.error="explicit initial source-refresh bootstrap admission required";return out;}
    if(p.output_sample_rate!=48000.||caller.time.output_block_count<1||
       static_cast<std::size_t>(caller.time.output_block_count)!=p.output_count||
       caller.source_refresh_budget!=std::min(139,caller.time.output_block_count)||
       caller.rate_external_budget<1||caller.rate_external_budget>100000000){
        out.error="bounded SourceS render requires Fo48k, matching explicit block count and native min(139,N) source budget";return out;
    }
    const auto context=derive_source_time_context(caller.time);
    if(!context.ok){out.error=context.error;return out;}
    const auto initial=evaluate_identity_source_point(source_time,context.element_relative_begin,0,true);
    const double start_samples=mul(initial.value,p.source_sample_rate);
    if(!initial.ok||!std::isfinite(start_samples)||start_samples<0||start_samples>static_cast<double>(pcm.size())){
        out.error="initial source curve position outside admitted component PCM extent";return out;
    }
    ExplicitSourceRateControl control(source_time,playback,p,caller);
    auto parameters=p;parameters.pitch_ratio=1.;parameters.initial_source_position=start_samples;
    out.renderer=detail::render_period_component_controlled(pcm,periods,parameters,&control);
    out.renderer.rate_controls=std::move(control.rate_events);out.renderer.final_control_count=control.rate.remaining;
    out.final_source_state=control.source;out.final_caller_time=control.time;
    out.rate_refreshes=control.rate.refreshes;out.source_events=std::move(control.source_events);
    out.ok=out.renderer.ok;out.error=out.renderer.error;return out;
}
} // namespace melodyne_re
