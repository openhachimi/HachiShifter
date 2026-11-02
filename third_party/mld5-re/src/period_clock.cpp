#include "period_clock.hpp"
#include <cmath>
#include <limits>

namespace melodyne_re {
namespace {
bool finite_positive(double x) { return std::isfinite(x) && x > 0; }
bool int32_conversion(double x) {
    return std::isfinite(x) && x >= std::numeric_limits<std::int32_t>::min()
        && x < static_cast<double>(std::numeric_limits<std::int32_t>::max()) + 1;
}
bool int64_conversion(double x) {
    return std::isfinite(x) && x >= -9223372036854775808.0
        && x < 9223372036854775808.0;
}
}
PeriodFramePlan plan_period_frame(const PeriodClockState& s,
                                 const PeriodClockParameters& p,
                                 const SourcePeriod& sample) {
    PeriodFramePlan out;
    out.next_state = s;
    auto fail = [&](const char* why) { out.ok = false; out.error = why; return out; };
    if (!finite_positive(p.source_sample_rate) || !finite_positive(p.output_sample_rate)
        || (p.source_stride != 1 && p.source_stride != 2 && p.source_stride != 4)
        || p.buffer_capacity < 2 || s.previous_span <= 0 || s.current_span <= 0
        || s.previous_span > p.buffer_capacity || s.current_span > p.buffer_capacity
        || !finite_positive(s.rate) || !std::isfinite(s.previous_period) || s.previous_period < 0
        || !std::isfinite(s.rate_delta) || !std::isfinite(s.source_step)
        || !int64_conversion(s.source_position) || !std::isfinite(s.read_position)
        || !finite_positive(s.threshold))
        return fail("invalid or unsupported explicit clock state");
    out.ok = true;
    out.due = s.read_position >= s.threshold; // 1819FECB3
    if (!out.due) return out;
    if (!sample) return fail("explicit source-period sampler required");
    out.shift_samples = s.previous_span;
    out.left_length = s.current_span;
    const float stride = static_cast<float>(p.source_stride);
    // Native computes trunc(S) BEFORE optional source-reset rounding.
    const auto integer_s = static_cast<std::int64_t>(s.source_position);
    // Promote before arithmetic only to avoid native integer overflow for rejected extremes.
    const long double integer_query = static_cast<long double>(integer_s)
        + static_cast<long double>(p.source_stride) * s.current_span
        - static_cast<long double>(p.component_source_start);
    if (integer_query < -9223372036854775808.0L || integer_query >= 9223372036854775808.0L)
        return fail("source period query integer overflow");
    double query = static_cast<double>(static_cast<std::int64_t>(integer_query))
        * (1.0 / p.source_sample_rate);
    bool invalid_period = false;
    auto evaluate = [&]() -> float {
        ++out.period_queries;
        float value;
        try { value = sample(query); }
        catch (...) { invalid_period = true; return 0; }
        if (!finite_positive(value)) invalid_period = true;
        return value;
    };
    float period = evaluate();
    if (invalid_period) return fail("period sampler failed or returned nonpositive/nonfinite P");
    out.initial_period = period;
    out.source_hz = static_cast<float>(p.source_sample_rate /
        ((static_cast<double>(period) + s.previous_period) * 0.5));
    out.target_ratio = static_cast<float>(s.rate /
        ((p.source_sample_rate / stride) * (1.0 / p.output_sample_rate)));
    // Explicit float P/D, then promote to double, matching DIVSS -> CVTPS2PD.
    const float grid_period = period / stride;
    out.predicted_source_position = s.source_position
        + (static_cast<double>(grid_period) / s.rate) * s.source_step;
    if (!std::isfinite(out.predicted_source_position)) return fail("predicted source overflow");
    double candidate = static_cast<double>(s.source_origin)
        + static_cast<double>(p.source_stride * static_cast<std::int64_t>(s.previous_span));
    auto origin = s.source_origin;
    if (s.source_reset) {
        candidate = s.source_position;
        if (!int32_conversion(s.source_position + 0.5)) return fail("reset origin exceeds native int32 range");
        origin = static_cast<std::int32_t>(s.source_position + 0.5);
    }
    int hops = 1;
    std::size_t corrections = 0;
    const double inverse_fs = 1.0 / p.source_sample_rate;
    // Native lower branch is do-while, but forward stops after one backward correction.
    if (static_cast<double>(period * 0.4f) + candidate > out.predicted_source_position) {
        do {
            if (corrections++ >= p.correction_budget) return fail("source correction budget exceeded");
            candidate -= static_cast<double>(period);
            query -= static_cast<double>(period) * inverse_fs;
            period = evaluate();
            if (invalid_period) return fail("period sampler failed during backward correction");
            if (hops == std::numeric_limits<int>::min()) return fail("source hop count overflow");
            --hops;
        } while (!p.forward && static_cast<double>(period * 0.4f) + candidate > out.predicted_source_position);
        // Original jumps over forward correction after this branch (hops!=1).
    } else if (out.predicted_source_position > static_cast<double>(period * 1.6f) + candidate) {
        do {
            if (corrections++ >= p.correction_budget) return fail("source correction budget exceeded");
            candidate += static_cast<double>(period);
            query += static_cast<double>(period) * inverse_fs;
            period = evaluate();
            if (invalid_period) return fail("period sampler failed during forward correction");
            if (hops == std::numeric_limits<int>::max()) return fail("source hop count overflow");
            ++hops;
        } while (out.predicted_source_position > static_cast<double>(period * 1.6f) + candidate);
    }
    out.source_hop_count = hops;
    out.final_period = period;
    out.final_period_query_seconds = query;
    if (hops != 1 && p.bridge_on_nonunit_hop)
        return fail("native this+632 bridge required; explicitly unsupported");
    int next_span = s.current_span;
    if (hops != 0) {
        const float rounded_span = period / stride + 0.5f;
        if (!int32_conversion(rounded_span) || !int32_conversion(candidate + 0.5))
            return fail("selected period/origin exceeds native int32 range");
        next_span = static_cast<int>(rounded_span);
        // Native is trunc(candidate+.5), including for negative origins, NOT round().
        origin = static_cast<std::int32_t>(candidate + 0.5);
    }
    while (next_span > p.buffer_capacity / 2) next_span /= 2;
    if (next_span <= 0 || s.current_span > p.buffer_capacity - next_span)
        return fail("zero hop or frame support outside capacity");
    out.right_length = next_span;
    out.selected_source_origin = origin;
    out.full_rebuild = p.do_fft || p.do_resample || hops != 1
        || s.previous_right_window_done || s.source_reset;
    out.fetch_destination_offset = out.full_rebuild ? 0 : s.current_span;
    const long double fetch_origin = static_cast<long double>(origin)
        + static_cast<long double>(p.source_stride) * out.fetch_destination_offset;
    if (fetch_origin < -9223372036854775808.0L || fetch_origin >= 9223372036854775808.0L)
        return fail("source fetch origin overflow");
    out.fetch_source_origin = static_cast<std::int64_t>(fetch_origin);
    out.fetch_count = out.full_rebuild ? s.current_span + next_span : next_span;
    out.new_left_window_requested = out.full_rebuild;
    out.old_left_window_requested = out.full_rebuild && !s.previous_right_window_done;
    // Successful FFT/resample takes 1819FF5FD/603 -> 1819FF65B:
    // var_F37 selects whole-support ADD, var_F2E requests the right window.
    // This is distinct from an unprocessed full fetch due to a source-hop/reset.
    const bool processed_support = p.do_fft || p.do_resample;
    out.new_right_window_requested = processed_support;
    // 1819FFC36 -> 1819FFCDE -> 181A025FA bypasses ADD for incremental fetch.
    // 181A0259B..25B0 selects L+H for processed support; 181A025FA skips tail COPY.
    out.add_prefix_count = out.full_rebuild ? s.current_span + (processed_support ? next_span : 0) : 0;
    out.copy_tail_offset = s.current_span;
    out.copy_tail_count = processed_support ? 0 : next_span;
    out.next_state.source_origin = origin;
    out.next_state.previous_period = static_cast<double>(out.initial_period);
    out.next_state.source_reset = false;
    out.next_state.previous_span = s.current_span;
    out.next_state.threshold = static_cast<double>(s.current_span);
    out.next_state.current_span = next_span;
    out.next_state.read_position = s.read_position - s.threshold;
    // Actual right handled result is returned by caller's frame processor.
    out.next_state.previous_right_window_done = false;
    return out;
}
void set_period_frame_right_window_done(PeriodFramePlan& plan, bool done) noexcept {
    if (plan.ok && plan.due) plan.next_state.previous_right_window_done = done;
}
bool read_period_linear(const std::vector<float>& rolling, double position,
                        float& output) noexcept {
    if (!int32_conversion(position) || position < 0) return false;
    const auto index = static_cast<std::size_t>(static_cast<std::int32_t>(position));
    if (index >= rolling.size() || rolling.size() - index < 2) return false;
    const float fraction = static_cast<float>(position - static_cast<double>(index));
    const float difference = rolling[index + 1] - rolling[index];
    const float product = difference * fraction;
    output = product + rolling[index];
    return true;
}
bool advance_period_output_clock(PeriodClockState& s) noexcept {
    const double next_source = s.source_position + s.source_step;
    const double next_rate = s.rate + s.rate_delta;
    const double next_read = s.read_position + next_rate;
    if (!std::isfinite(next_source) || !std::isfinite(next_rate) || !std::isfinite(next_read)) return false;
    s.source_position = next_source;
    s.rate = next_rate;
    s.read_position = next_read;
    return true;
}
int native_period_buffer_capacity(double output_sample_rate) noexcept {
    if (!finite_positive(output_sample_rate)) return 0;
    return output_sample_rate<75000. ? 4096 : output_sample_rate<150000. ? 8192 : 16384;
}
PeriodStartupPlan plan_period_startup(const PeriodStartupParameters& p,
                                     const StartupSourcePeriod& sample) {
    PeriodStartupPlan out;
    auto fail = [&](const char* why) { out.ok=false; out.error=why; return out; };
    if (!finite_positive(p.source_sample_rate) || !finite_positive(p.rate)
        || !std::isfinite(p.source_step) || !std::isfinite(p.rate_delta)
        || !int64_conversion(p.source_position) || p.source_position < 0
        || !finite_positive(p.q) || !finite_positive(p.target_pitch_ratio)
        || p.processing_capacity < 2 || p.rolling_capacity < 2
        || p.processing_capacity > std::numeric_limits<int>::max()-4
        || p.rolling_capacity > std::numeric_limits<int>::max()-4
        || (p.source_stride!=1 && p.source_stride!=2 && p.source_stride!=4))
        return fail("invalid or unsupported explicit startup state/capacities");
    if (p.reference_enabled) return fail("reference startup requires constructor phase state; unsupported");
    out.startup_q = p.q < 1.f ? p.q : 1.f;
    const bool processed = out.startup_q < .999f || out.startup_q > 1.001f;
    if (processed && p.source_stride!=1)
        return fail("processed startup D>1 requires additional native validation; unsupported");
    const auto integer_source = static_cast<std::int64_t>(p.source_position);
    out.initial_clear_count = p.rolling_capacity+1; // includes left guard
    out.state.source_position=p.source_position;
    out.state.source_step=p.source_step;
    out.state.rate=p.rate;
    out.state.rate_delta=p.rate_delta;
    out.state.read_position=p.source_position-static_cast<double>(integer_source);
    out.state.source_origin=integer_source; // caller 1819FDD57 overrides helper's locals
    out.state.source_reset=p.source_reset;
    out.state.previous_right_window_done=false;
    bool bad=false;
    auto query_period = [&](double q) {
        double value=p.source_sample_rate/100.;
        if (sample) {
            out.period_queries.push_back(q);
            try { value=sample(q); } catch (...) { bad=true; }
        }
        if (!finite_positive(value)) bad=true;
        return value;
    };
    auto round_positive = [&](double value) -> int {
        if (!int32_conversion(value+.5) || value <= 0) {bad=true;return 0;}
        return static_cast<int>(value+.5);
    };
    auto narrow_origin = [&](long double value) -> std::int64_t {
        if (value < -9223372036854775808.0L || value >= 9223372036854775808.0L) {
            bad=true;return 0;
        }
        return static_cast<std::int64_t>(value);
    };
    const double inverse_fs=1./p.source_sample_rate;
    const double relative=p.source_position-static_cast<double>(p.component_source_start);
    int previous=0,current=0;
    if (!processed) {
        const double first=query_period(relative*inverse_fs);
        const double second=query_period((relative+first)*inverse_fs);
        if (bad) return fail("invalid startup period callback");
        previous=round_positive(first/static_cast<double>(p.source_stride));
        current=round_positive(second/static_cast<double>(p.source_stride));
        out.state.previous_period=second;
        if (bad) return fail("startup period/span conversion outside admitted range");
        while (previous>p.processing_capacity/2 || current>p.processing_capacity/2) {
            previous/=2;current/=2;
        }
        if (previous<=0 || current<=0 || previous>p.rolling_capacity-current)
            return fail("startup support outside rolling capacity");
        out.raw_prefetch=true;
        out.raw_fetch_source_origin=narrow_origin(static_cast<long double>(integer_source)-p.source_stride);
        out.raw_fetch_count=previous+current+1;
        if (p.end_taper_active) {
            const long double distance=static_cast<long double>(p.component_source_end)-integer_source;
            if (distance>=0 && distance<previous+current) {
                out.end_taper_begin=static_cast<int>(distance);
                out.end_taper_count=previous+current-out.end_taper_begin;
                const float logarithm=std::log(0.5f);
                out.end_taper_ratio=std::exp(logarithm/35.f);
            }
        }
        if (bad) return fail("startup source origin overflow");
    } else {
        // Fully allocated Q<1 branch: 1819F5Cxx..1819F6177, exactly three frames.
        double query_samples=relative-1.;
        std::int64_t metadata_position=narrow_origin(static_cast<long double>(integer_source)-1);
        std::int64_t fetch_origin=metadata_position;
        int placement=-1, last_right=0;
        for (int frame_index=0;frame_index<3;++frame_index) {
            PeriodStartupFrame frame;
            frame.period_query_seconds=query_samples*inverse_fs;
            frame.source_period=query_period(frame.period_query_seconds);
            if (bad) return fail("invalid processed startup period callback");
            frame.right_length=round_positive(frame.source_period);
            frame.left_length=frame_index==0?frame.right_length:last_right;
            if (bad || frame.left_length<=0 || frame.right_length<=0
                || frame.left_length>p.processing_capacity-frame.right_length)
                return fail("processed startup frame exceeds capacity before native post-halving");
            if (frame_index==0)
                fetch_origin=narrow_origin(static_cast<long double>(fetch_origin)-frame.left_length);
            frame.fetch_source_origin=fetch_origin;
            frame.fetch_count=frame.left_length+frame.right_length;
            frame.component_source_position=metadata_position;
            frame.source_hz=static_cast<float>(p.source_sample_rate/frame.source_period);
            frame.fft_size=16;
            while (frame.fft_size<frame.fetch_count && frame.fft_size<4096) frame.fft_size*=2;
            frame.bin_hz=(static_cast<float>(p.source_sample_rate)/static_cast<float>(p.source_stride))
                /static_cast<float>(frame.fft_size);
            frame.float_period=static_cast<float>(frame.source_period);
            frame.target_pitch_ratio=p.target_pitch_ratio;
            frame.q=out.startup_q;
            frame.add_destination_offset=placement-frame.left_length;
            frame.add_source_offset=0;
            frame.add_count=frame.fetch_count;
            if (frame.add_destination_offset < -1) {
                frame.add_source_offset=-1-frame.add_destination_offset;
                frame.add_count+=frame.add_destination_offset+1;
                frame.add_destination_offset=-1;
            }
            if (frame.add_count>p.rolling_capacity-frame.add_destination_offset)
                frame.add_count=p.rolling_capacity-frame.add_destination_offset;
            if (frame.add_count<0 || bad) return fail("processed startup placement outside guarded capacity");
            out.frames.push_back(frame);
            if (frame_index==0) previous=round_positive(frame.source_period);
            if (frame_index==1) {
                current=round_positive(frame.source_period);
                out.state.previous_period=frame.source_period; // SECOND period, not third
            }
            query_samples+=static_cast<double>(frame.right_length);
            metadata_position=narrow_origin(static_cast<long double>(metadata_position)+frame.right_length);
            fetch_origin=narrow_origin(static_cast<long double>(fetch_origin)+frame.right_length);
            if (placement>std::numeric_limits<int>::max()-frame.right_length)
                return fail("startup placement integer overflow");
            placement+=frame.right_length;
            last_right=frame.right_length;
        }
        if (bad) return fail("processed startup integer overflow");
        while (previous>p.processing_capacity/2 || current>p.processing_capacity/2) {
            previous/=2;current/=2;
        }
        if (previous<=0 || current<=0 || previous>p.rolling_capacity-current)
            return fail("processed startup final spans outside rolling capacity");
        out.tail_clear_begin=previous+current;
        out.tail_clear_count=p.rolling_capacity-out.tail_clear_begin;
    }
    out.state.previous_span=previous;
    out.state.current_span=current;
    out.state.threshold=static_cast<double>(previous);
    out.ok=true;
    return out;
}
} // namespace melodyne_re
