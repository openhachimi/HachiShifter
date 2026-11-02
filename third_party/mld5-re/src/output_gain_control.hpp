#pragma once
#include <cstdint>
#include <limits>
#include "source_time_control.hpp"
namespace melodyne_re {
struct OutputGainState {
    float value=1.0f;
    float delta=0.0f;
    std::int32_t gain_remaining=0;   // native gain smoother+24
    std::int32_t source_remaining=0; // native source smoother+32
    double threshold=std::numeric_limits<double>::max();
    bool fading=false;              // renderer+616
};
struct OutputGainThreshold {
    bool ok=false;
    double value=0;
    const char* error=nullptr;
};
// Native FEC33..FECB3, with source.v1112 already resolved by caller.
OutputGainThreshold calculate_component_shutdown_threshold(double source_sample_rate,
    double signal_end,double source_step,bool source_feature,bool period_path) noexcept;
// Native prebody sentinel guard: DBL_MAX skips recomputation altogether.
OutputGainThreshold refresh_component_shutdown_threshold(OutputGainState& state,
    double source_sample_rate,double signal_end,double source_step,
    bool source_feature,bool period_path) noexcept;
struct OutputGainTransition {
    bool ok=false;
    bool threshold_consumed=false;
    bool shutdown_started=false;
    bool freeze_source_refresh=false;
    bool request_stop=false;
    std::int32_t nominal_fade_samples=0; // diagnostic divisor, NOT an output budget
    const char* error=nullptr;
};
// Native C96..D46. Source coordinate and gain MUST ALREADY be advanced using
// their OLD deltas for the just-emitted sample. This does not advance clocks.
// Queue-empty is explicit caller input, not inferred from a score/project.
OutputGainTransition component_shutdown_after_advance(OutputGainState& state,
    double source_after_advance,double source_step,double source_sample_rate,
    bool inhibit_322,bool pending_queue_empty) noexcept;
struct OutputGainValue {bool ok=false;float value=0;const char* error=nullptr;};
// Plain (non-soft-clipping) native2960 multiply. Emit with CURRENT gain before
// advance_output_gain_value, then call component_shutdown_after_advance.
OutputGainValue multiply_output_gain(float sample,const OutputGainState& state) noexcept;
// Original per-output float addition only. Counter/block bookkeeping remains
// caller-owned; native+24/+32 are NOT decremented by this instruction group.
OutputGainValue advance_output_gain_value(OutputGainState& state) noexcept;

// Bounded usable SourceTime+gain composition. Initial gain1/delta0, period608,
// clipfalse and no other release/deadline branch. Conditions are caller data,
// never selected from an audio score or project guessed inside this module.
struct OutputGainRenderParameters {
    double signal_end=0; // explicit positive absolute source-sample endpoint
    bool source_feature=false;
    bool inhibit_322=false;
    bool pending_queue_empty=false;
    bool last_component_zero_remainder=false;
};
struct OutputGainRenderRecords {
    double initial_threshold=std::numeric_limits<double>::max();
    bool shutdown_started=false;
    std::size_t trigger_after_output=std::numeric_limits<std::size_t>::max();
    double trigger_source_after=0;
    double trigger_source_step=0;
    std::int32_t fade_divisor=0;
    bool stop_requested=false;
    std::size_t stop_after_output=std::numeric_limits<std::size_t>::max();
    std::vector<float> emitted_gain; // CURRENT gain from this same raw run
    OutputGainState final_gain;
};
struct OutputGainRenderResult {
    bool ok=false;
    std::string error;
    SourceTimeRenderResult raw; // regenerated with actual source-refresh freeze
    std::vector<float> samples; // plain per-component gain, no sibilant/mix/converter
    OutputGainRenderRecords records;
    // Raw controller still computes through the supplied block after a bounded
    // zero-remainder stop; it is never a native virtual-cleanup state claim.
    bool native_cleanup_equivalent=false;
};
// Preserves/chains a caller read-only before observer. Requires no pre-existing
// after-output action (one freeze owner). No paths, parsing, fitting or GUI.
OutputGainRenderResult render_period_component_source_time_gain(
    const std::vector<float>& pcm,const SampledFunctionView& periods,
    const SampledFunctionView& playback,IdentitySourceTimeView source_time,
    const PeriodRendererParameters& parameters,const SourceTimeRenderContext& caller,
    const OutputGainRenderParameters& gain);
} // namespace melodyne_re
