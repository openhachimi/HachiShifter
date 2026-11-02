#pragma once
#include "sampled_function.hpp"
#include "period_clock.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace melodyne_re {
// Restricted ordinary608 component renderer. Explicit component PCM is NOT the
// original source unless the caller actually selects that source. No detector.
struct PeriodRendererParameters {
    double source_sample_rate = 32000;
    double output_sample_rate = 48000;
    std::size_t output_count = 27648;
    double pitch_ratio = 1.0; // externally supplied 2^(semitones/12)
    float probe_ratio = 1.0f; // actual Q; no unknown override silently inferred
    double initial_source_position = 0.0;
    bool renderer_parameter57 = false;
    bool external_bridge_flag = false;
    double time_stretch_ratio = 1.0; // this composition currently admits identity only
    bool reference_phase = false;
    bool clip_output = false; // native ordinary caller false; opt-in comparison
    float clip_limit = 1.0f;
    bool end_taper_active = false;
};
struct PeriodRendererEvent {
    std::size_t output_index = 0;
    bool startup = false;
    bool full_rebuild = false;
    std::int64_t fetch_origin = 0;
    int fetch_count = 0;
    int left_length = 0;
    int right_length = 0;
    int add_count = 0;
    int copy_count = 0;
    int source_hops = 0;
    double source_position = 0;
    double read_position = 0;
};
struct PeriodRateControlEvent {
    std::size_t output_index = 0;
    double source_seconds = 0;
    double source_step_seconds = 0;
    int requested_count = 0;
    int returned_count = 0;
    int cached_index = 0;
    double accessor_endpoint = 0;
    float endpoint_pitch = 0;
    double current_rate = 0;
    double endpoint_rate = 0;
    double delta = 0;
};
struct PeriodRendererResult {
    bool ok = false;
    std::string error;
    std::vector<float> samples;
    std::vector<PeriodRendererEvent> events;
    PeriodClockState final_state;
    bool processed_startup = false;
    std::vector<PeriodRateControlEvent> rate_controls; // cached path only
    int final_control_count = 0;
};
// D1 mono float PCM, zero padding outside [0,pcm.size), identity source-time,
// scalar constant controls. Ordinary v1206=false restriction is explicit.
// No amplitude-envelope reapplication, output normalization, EQ or gain fit.
PeriodRendererResult render_period_component(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const PeriodRendererParameters& parameters);
// New path: cached local playback pitch with native860/9D0 rate ramping.
// Q remains p.probe_ratio CONSTANT. p.pitch_ratio is not used for the cached R.
// Positive request budget is an explicit caller context bound, not recovered
// application control-manager policy. Each refresh also bounds by render tail.
PeriodRendererResult render_period_component_cached(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const SampledFunctionView& playback,
    const PeriodRendererParameters& parameters,int requested_count_budget);
namespace detail {
// Internal extension seam keeps old scalar clients linkable without the new
// cached_control translation unit. No alternative DSP/window/OLA implementations.
struct PeriodRendererRateControl {
    virtual ~PeriodRendererRateControl() = default;
    virtual bool initialize(PeriodStartupParameters& startup,const char*& error) = 0;
    virtual bool before_output(PeriodClockState& state,std::size_t output_index,
                               std::size_t remaining_outputs,const char*& error) = 0;
    virtual bool after_output(const PeriodClockState& state,const char*& error) = 0;
    // Hachi host extension: explicit frame Q automation. Existing recovered
    // compositions retain their original constant Q through this default.
    virtual float frame_probe_ratio(float constant_q) const { return constant_q; }
    virtual bool requires_processed_frames() const noexcept { return false; }
};
PeriodRendererResult render_period_component_controlled(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const PeriodRendererParameters& parameters,
    PeriodRendererRateControl* rate_control);
}
} // namespace melodyne_re
