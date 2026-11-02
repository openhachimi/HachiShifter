#include "period_renderer.hpp"
#include "spectral_frame.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

#if defined(__FAST_MATH__)
#error "Recovered period composition requires no fast-math"
#endif
namespace melodyne_re {
PeriodRendererResult detail::render_period_component_controlled(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const PeriodRendererParameters& p,
    detail::PeriodRendererRateControl* rate_control) {
    PeriodRendererResult out;
    auto fail=[&](const char* why) {out.error=why;out.ok=false;return out;};
    if(pcm.empty() || !periods.values || periods.count<2)
        return fail("explicit component PCM and sampled period array required");
    if(!std::isfinite(p.source_sample_rate) || p.source_sample_rate<=0 ||
       !std::isfinite(p.output_sample_rate) || p.output_sample_rate<=0 ||
       !std::isfinite(p.pitch_ratio) || p.pitch_ratio<=0 ||
       !std::isfinite(p.probe_ratio) || p.probe_ratio<=0 ||
       !std::isfinite(p.initial_source_position) || p.initial_source_position<0 ||
       p.initial_source_position>static_cast<double>(pcm.size()) ||
       p.output_count>100000000 || !std::isfinite(p.clip_limit) || p.clip_limit<=0)
        return fail("invalid explicit renderer scalar controls");
    if(p.renderer_parameter57 || p.external_bridge_flag || p.time_stretch_ratio!=1.0 || p.reference_phase)
        return fail("only ordinary608 identity-time v1206=false, reference-disabled path admitted");
    for(float x:pcm)if(!std::isfinite(x))return fail("nonfinite component PCM");
    const int capacity=native_period_buffer_capacity(p.output_sample_rate);
    if(capacity<=0)return fail("invalid native capacity");
    const RaisedCosineKernel kernel;
    const FrameWindowTables tables(kernel);
    // Storage slot0 is logical[-1]; slots1..capacity are logical[0..capacity-1].
    std::vector<float> rolling(static_cast<std::size_t>(capacity)+1,0.0f);
    auto at=[&](int i)->float& {return rolling[static_cast<std::size_t>(i+1)];};
    auto fetch=[&](std::vector<float>& destination,int offset,std::int64_t origin,int count)->bool {
        if(offset<0 || count<0 || offset>static_cast<int>(destination.size())-count)return false;
        for(int j=0;j<count;++j) {
            if(origin>std::numeric_limits<std::int64_t>::max()-j)return false;
            const auto index=origin+j;
            destination[static_cast<std::size_t>(offset+j)]=index>=0 &&
                static_cast<std::uint64_t>(index)<pcm.size()?pcm[static_cast<std::size_t>(index)]:0.0f;
        }
        return true;
    };
    auto sample_double=[&](double x)->double {
        const auto value=evaluate_sampled_function(periods,x,false);
        if(!value.ok)throw std::runtime_error(value.error);
        return value.value;
    };
    auto sample_float=[&](double x)->float {return static_cast<float>(sample_double(x));};
    auto frame=[&](std::int64_t origin,int count,int destination,int left,int right,
                   float source_hz,float q,bool do_fft,bool left_requested,bool right_requested,
                   std::vector<float>& result,bool& right_done)->const char* {
        // Main1819FB2E0 clears complete work capacity before EVERY source fetch.
        std::vector<float> work(static_cast<std::size_t>(capacity),0.0f);
        if(!fetch(work,destination,origin,count))return "frame source read outside work capacity";
        FrameTransformParameters fp;
        fp.left_length=left;fp.right_length=right;fp.ratio=1.0f; // a6=rho, NOT Q
        fp.do_fft=do_fft;fp.do_resample=false;fp.cubic=true;
        fp.left_window_requested=left_requested;fp.right_window_requested=right_requested;
        if(do_fft) {
            SpectralFrameParameters sp;sp.frame=fp;sp.source_hz=source_hz;
            sp.processing_sample_rate=static_cast<float>(p.source_sample_rate);
            sp.probe_ratio=q;sp.output_gain=1.0f;
            auto r=process_spectral_frame_mono(work,sp,tables,kernel);
            if(!r.ok)return r.error;
            result=std::move(r.samples);right_done=r.right_window_done;
        } else {
            auto r=transform_frame_no_fft({work},fp,tables);
            if(!r.ok)return r.error;
            result=std::move(r.channels.front());right_done=r.right_window_done;
        }
        return nullptr;
    };
    PeriodStartupParameters startup;
    startup.source_sample_rate=p.source_sample_rate;startup.source_stride=1;
    startup.processing_capacity=capacity;startup.rolling_capacity=capacity;
    startup.source_position=p.initial_source_position;
    // Source-time Bezier identity => source clock advances Fs/Fo each output.
    startup.source_step=p.source_sample_rate/p.output_sample_rate;
    startup.rate=startup.source_step*p.pitch_ratio;
    startup.q=p.probe_ratio;startup.target_pitch_ratio=static_cast<float>(p.pitch_ratio);
    startup.end_taper_active=p.end_taper_active;
    startup.component_source_end=static_cast<std::int64_t>(pcm.size());
    if(rate_control) {
        const char* error=nullptr;
        if(!rate_control->initialize(startup,error))return fail(error?error:"cached rate initialization failed");
    }
    auto plan=plan_period_startup(startup,sample_double);
    if(!plan.ok)return fail(plan.error);
    if(plan.raw_prefetch) {
        if(!fetch(rolling,plan.raw_fetch_destination_offset+1,plan.raw_fetch_source_origin,plan.raw_fetch_count))
            return fail("raw startup fetch outside guarded rolling capacity");
        float amplitude=1.0f;
        for(int j=0;j<plan.end_taper_count;++j) {
            const int dest=plan.end_taper_begin+j;
            if(dest<0 || dest>=capacity)return fail("startup taper outside rolling capacity");
            at(dest)=at(dest)*amplitude;amplitude=amplitude*plan.end_taper_ratio;
        }
        PeriodRendererEvent e;e.startup=true;e.fetch_origin=plan.raw_fetch_source_origin;
        e.fetch_count=plan.raw_fetch_count;e.source_position=startup.source_position;
        e.left_length=plan.state.previous_span;e.right_length=plan.state.current_span;
        out.events.push_back(e);
    } else {
        out.processed_startup=true;
        for(const auto& f:plan.frames) {
            std::vector<float> processed;bool done=false;
            const char* error=frame(f.fetch_source_origin,f.fetch_count,0,f.left_length,f.right_length,
                f.source_hz,f.q,f.do_fft,f.left_window_requested,f.right_window_requested,processed,done);
            if(error)return fail(error);
            for(int j=0;j<f.add_count;++j) {
                const int dest=f.add_destination_offset+j,src=f.add_source_offset+j;
                if(dest< -1 || dest>=capacity || src<0 || src>=capacity)return fail("startup ADD outside guarded buffers");
                at(dest)=at(dest)+processed[static_cast<std::size_t>(src)];
            }
            PeriodRendererEvent e;e.startup=true;e.full_rebuild=true;e.fetch_origin=f.fetch_source_origin;
            e.fetch_count=f.fetch_count;e.left_length=f.left_length;e.right_length=f.right_length;
            e.add_count=f.add_count;e.source_position=static_cast<double>(f.component_source_position);
            out.events.push_back(e);
        }
        for(int j=0;j<plan.tail_clear_count;++j)at(plan.tail_clear_begin+j)=0.0f;
    }
    auto state=plan.state;
    PeriodClockParameters clock;
    clock.source_sample_rate=p.source_sample_rate;clock.output_sample_rate=p.output_sample_rate;
    clock.source_stride=1;clock.buffer_capacity=capacity;clock.forward=true;
    clock.bridge_on_nonunit_hop=false;clock.do_resample=false;
    clock.do_fft=p.probe_ratio<.999f || p.probe_ratio>1.001f;
    out.samples.reserve(p.output_count);
    for(std::size_t n=0;n<p.output_count;++n) {
        if(rate_control) {
            const char* error=nullptr;
            if(!rate_control->before_output(state,n,p.output_count-n,error))
                return fail(error?error:"cached rate segment refresh failed");
        }
        const float frame_q=rate_control?rate_control->frame_probe_ratio(p.probe_ratio):p.probe_ratio;
        if(!std::isfinite(frame_q)||frame_q<=0)return fail("invalid host frame Q");
        clock.do_fft=(rate_control&&rate_control->requires_processed_frames())||frame_q<.999f||frame_q>1.001f;
        auto event=plan_period_frame(state,clock,sample_float);
        if(!event.ok)return fail(event.error);
        if(event.due) {
            const int shift=event.shift_samples;
            if(shift<1 || shift>capacity)return fail("invalid guarded rolling shift");
            // Copy logical[shift-1..capacity-1] ->[-1..capacity-shift-1].
            std::memmove(rolling.data(),rolling.data()+shift,
                static_cast<std::size_t>(capacity-shift+1)*sizeof(float));
            std::fill(rolling.begin()+(capacity-shift+1),rolling.end(),0.0f);
            std::vector<float> processed;bool right_done=false;
            const char* error=frame(event.fetch_source_origin,event.fetch_count,event.fetch_destination_offset,
                event.left_length,event.right_length,event.source_hz,frame_q,clock.do_fft,
                event.new_left_window_requested,event.new_right_window_requested,processed,right_done);
            if(error)return fail(error);
            if(event.old_left_window_requested) {
                const float step=8192.0f/static_cast<float>(event.left_length);float phase=0.0f;
                for(int j=0;j<event.left_length;++j) {
                    const int index=static_cast<int>(phase)+8192;
                    if(index<8192 || index>16384)return fail("old rolling Hann index outside table");
                    at(j)=tables.hann()[static_cast<std::size_t>(index)]*at(j);
                    phase=phase+step;
                }
            }
            for(int j=0;j<event.add_prefix_count;++j)at(j)=at(j)+processed[static_cast<std::size_t>(j)];
            for(int j=0;j<event.copy_tail_count;++j) {
                const int dest=event.copy_tail_offset+j;
                at(dest)=processed[static_cast<std::size_t>(dest)];
            }
            set_period_frame_right_window_done(event,right_done);
            state=event.next_state;
            PeriodRendererEvent e;e.output_index=n;e.full_rebuild=event.full_rebuild;
            e.fetch_origin=event.fetch_source_origin;e.fetch_count=event.fetch_count;
            e.left_length=event.left_length;e.right_length=event.right_length;
            e.add_count=event.add_prefix_count;e.copy_count=event.copy_tail_count;
            e.source_hops=event.source_hop_count;e.source_position=state.source_position;
            e.read_position=state.read_position;out.events.push_back(e);
        }
        if(!std::isfinite(state.read_position) || state.read_position<0 || state.read_position>=capacity-2)
            return fail("cubic emission outside guarded rolling support");
        const int i=static_cast<int>(state.read_position);
        const float fraction=static_cast<float>(state.read_position-static_cast<double>(i));
        float value=lagrange_cubic(at(i-1),at(i),at(i+1),at(i+2),fraction);
        if(p.clip_output)value=std::fmax(-p.clip_limit,std::fmin(p.clip_limit,value));
        if(!std::isfinite(value))return fail("nonfinite emitted sample");
        out.samples.push_back(value);
        if(!advance_period_output_clock(state))return fail("output clock overflow");
        if(rate_control) {
            const char* error=nullptr;
            if(!rate_control->after_output(state,error))return fail(error?error:"cached rate consumption failed");
        }
    }
    out.final_state=state;out.ok=true;return out;
}
PeriodRendererResult render_period_component(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const PeriodRendererParameters& parameters) {
    return detail::render_period_component_controlled(pcm,periods,parameters,nullptr);
}
} // namespace melodyne_re
