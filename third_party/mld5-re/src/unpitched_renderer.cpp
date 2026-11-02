#include "unpitched_renderer.hpp"
#include "period_clock.hpp"
#include "primitives.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#if defined(__FAST_MATH__)
#error "Recovered unpitched arithmetic requires no fast-math"
#endif
namespace melodyne_re {
namespace {
int band(double rate) {return rate<75000.?1:rate<150000.?2:4;}
}
UnpitchedSibilantResult render_unpitched_sibilant_mono(
    const std::vector<float>& pcm,const UnpitchedSibilantParameters& p) {
    UnpitchedSibilantResult out;
    auto fail=[&](const char* message) {out.ok=false;out.error=message;return out;};
    if(!p.source_type_is_sibilant || !p.identity_time_mapping ||
       (!p.spectrum_shaper_absent && !p.neutral_shaper_gain_verified) || !p.gain_curve_absent)
        return fail("sibilant type/identity-time/neutral shape gain/absent gain curve require explicit evidence");
    if(p.global_pitch_ratio!=1 || p.global_formant_ratio!=1 ||
       p.source_gain_db!=0 || p.sibilant_balance!=0)
        return fail("only unity GLOBAL controls, zero source dB and zero sibilant balance admitted");
    if(pcm.empty() || pcm.size()>100000000 || p.output_count>100000000 ||
       !std::isfinite(p.source_sample_rate) || p.source_sample_rate<=0 ||
       !std::isfinite(p.output_sample_rate) || p.output_sample_rate<=0 ||
       !std::isfinite(p.local_note_pitch_ratio) || p.local_note_pitch_ratio<=0 ||
       !std::isfinite(p.initial_source_position) || p.initial_source_position<0 ||
       p.initial_source_position>static_cast<double>(pcm.size()))
        return fail("invalid PCM/scalar/count domain");
    if(p.native_tail_helper_disabled && p.native_tail_fade_enabled)
        return fail("contradictory endpoint helper conditions");
    const int source_band=band(p.source_sample_rate),output_band=band(p.output_sample_rate);
    const int stride=source_band/std::min(source_band,output_band);
    if(stride!=1) return fail("only D1 source fetch admitted");
    const int capacity=native_period_buffer_capacity(p.output_sample_rate);
    const int span=p.history_span;
    if(span<16 || span%4 || span>capacity) return fail("actual native frame span must be positive multiple-of-four within capacity");
    for(float value:pcm) if(!std::isfinite(value))return fail("nonfinite source PCM");
    const int hop=span/4,left=span-hop;
    const double step=p.source_sample_rate/p.output_sample_rate;
    if(!std::isfinite(step) || step<=0 || step>hop) return fail("source/output step outside bounded specialization");
    // Slot0 is logical[-1]. Native capacity includes no extra right samples.
    std::vector<float> rolling(static_cast<std::size_t>(capacity)+1,0.f);
    auto at=[&](int index)->float& {return rolling[static_cast<std::size_t>(index+1)];};
    auto fetch=[&](int offset,std::int64_t origin,int count,UnpitchedRefillEvent* event)->bool {
        if(offset< -1 || count<0 || offset>capacity-count) return false;
        if(origin>std::numeric_limits<std::int64_t>::max()-count) return false;
        const auto end=static_cast<std::int64_t>(pcm.size());
        const bool straddles=origin<end && origin+count>end;
        // Fully beyond-end read has shortened native count0; EFCC0(0) is a
        // no-op regardless of unknown predicate. Raw startup does not call it.
        if(event && straddles && !p.native_tail_helper_disabled && !p.native_tail_fade_enabled) return false;
        for(int i=0;i<count;++i) {
            const auto index=origin+i;
            at(offset+i)=index>=0 && static_cast<std::uint64_t>(index)<pcm.size()
                ?pcm[static_cast<std::size_t>(index)]:0.f;
        }
        if(event && origin+count>end) {
            const int valid=origin>=end?0:static_cast<int>(end-origin);
            event->fetch_count=valid;event->tail_zero_count=count-valid;
            if(p.native_tail_fade_enabled && valid>0) {
                const int length=std::min(valid,100),begin=offset+valid-length;
                const float decrement=1.f/static_cast<float>(length);float gain=1.f;
                for(int i=0;i<length;++i) {
                    gain=std::fmax(0.f,gain-decrement);
                    at(begin+i)=gain*at(begin+i);
                }
                event->tail_fade_begin=begin;event->tail_fade_count=length;
            }
        }
        return true;
    };
    double source=p.initial_source_position;
    auto origin=static_cast<std::int64_t>(source);
    double clock=source-static_cast<double>(origin);
    // 5560 non608 Qstartup1 prefetches N+1 into rolling[-1], origin trunc(S)-D.
    if(!fetch(-1,origin-1,span+1,nullptr))return fail("raw startup outside guarded source-copy domain");
    out.samples.reserve(p.output_count);
    for(std::size_t n=0;n<p.output_count;++n) {
        out.rejected_output_index=n;
        if(clock>=hop) {
            // Native identity-time source-center search, 1819FFDxx; preserve
            // scalar operation order rather than replacing by round(source/hop).
            const double predicted=static_cast<double>(span)/step*step*.5+source+.5;
            if(!std::isfinite(predicted) || predicted>=2147483647.0-2*span)
                return fail("source center outside native bounded integer domain");
            const auto center=static_cast<std::int64_t>(predicted);
            const auto old_origin=origin;
            auto candidate=origin+hop;
            auto distance=[&](std::int64_t b) {
                const auto d=center-(b+span/2);
                return d<0?-d:d;
            };
            auto previous=distance(origin),next=distance(candidate);
            int source_hops=0;
            while(next<previous) {
                origin=candidate;previous=next;++source_hops;
                if(source_hops>16)return fail("source search would exceed specialization budget");
                candidate+=hop;next=distance(candidate);
            }
            // Entering native phase/history for repeated/skipped source blocks
            // is NOT the pure raw interpolation specialization.
            if(source_hops!=1)return fail("native source-hop selection would activate spectral history");
            UnpitchedRefillEvent event;
            event.output_index=n;event.old_origin=old_origin;event.new_origin=origin;
            event.source_hops=source_hops;event.fetch_origin=origin+left;
            event.fetch_offset=left;event.fetch_count=hop;
            event.source_position=source;event.read_position_before_rollover=clock;
            // Shift by Hprev=Hcur=N/4, including the real logical[-1] guard.
            std::memmove(rolling.data(),rolling.data()+hop,
                static_cast<std::size_t>(capacity-hop+1)*sizeof(float));
            std::fill(rolling.begin()+(capacity-hop+1),rolling.end(),0.f);
            if(!fetch(left,event.fetch_origin,hop,&event))return fail("partial endpoint refill requires verified fade-enabled/disabled predicate");
            // No prefix ADD, no windows, no FFT; native jumps directly to tail copy.
            clock-=static_cast<double>(hop);
            out.events.push_back(event);
        }
        if(clock<0 || clock>=capacity-2 || !std::isfinite(clock))
            return fail("rolling cubic read outside guarded domain");
        const int i=static_cast<int>(clock);
        const float fraction=static_cast<float>(clock-static_cast<double>(i));
        const float value=lagrange_cubic(at(i-1),at(i),at(i+1),at(i+2),fraction);
        if(!std::isfinite(value))return fail("nonfinite interpolation output");
        // Actual neutral output gain is one; no limiter (621=0), EQ or noise.
        out.samples.push_back(value*1.f);
        source+=step;
        clock+=step; // preserve rolling carry; not recomputed from global source
    }
    out.final_source_position=source;out.final_read_position=clock;out.final_source_origin=origin;
    out.ok=true;return out;
}
} // namespace melodyne_re
