#pragma once
#include <cstddef>
#include <cstdint>
#include "period_renderer.hpp"
#include <string>
#include <vector>

namespace melodyne_re {
// Restricted proven fast curve: strictly increasing x knots, y=x exactly.
// Actual successful graph249 materialization retains its four original knots.
struct IdentitySourceTimeView {
    const double* knots = nullptr;
    std::size_t count = 0;
};
struct SourceTimeAccess {
    bool ok = false;
    double value = 0;
    int count = 0;
    int cached_index = 0;
    const char* error = nullptr;
};
SourceTimeAccess evaluate_identity_source_point(IdentitySourceTimeView curve,
    double x,int cached_index=0,bool extrapolate=true) noexcept;
SourceTimeAccess evaluate_identity_source_segment(IdentitySourceTimeView curve,
    double x,double dx,int requested_count,int cached_index=0,bool extrapolate=true) noexcept;
struct SourceTimeContext {
    double begin_time = 0;
    double end_time = 0;
    int output_block_count = 0;
    double element_start = 0;
    bool previous_element_start_valid = false;
    double previous_element_start = 0;
};
struct SourceTimeContextResult {
    bool ok = false;
    double element_relative_begin = 0;
    double output_time_step = 0; // (end-begin)*(1/blockCount), used by global time
    double accessor_time_step = 0; // start-change-adjusted var_E10
    const char* error = nullptr;
};
SourceTimeContextResult derive_source_time_context(const SourceTimeContext& context) noexcept;
struct SourceTimeControlParameters {
    double source_sample_rate = 32000;
    std::int64_t component_source_offset = 0;
};
struct SourceTimeControlState {
    bool initialized = false;
    double source_samples = 0;
    double source_step = 0;
    int remaining = 0;
    int cached_index = 0;
};
bool initialize_source_time_control(IdentitySourceTimeView curve,double element_seconds,
    const SourceTimeControlParameters& parameters,SourceTimeControlState& state,
    const char*& error) noexcept;
SourceTimeAccess refresh_source_time_control(IdentitySourceTimeView curve,
    double element_seconds,double accessor_time_step,int requested_count,
    const SourceTimeControlParameters& parameters,SourceTimeControlState& state) noexcept;
// Updates S+=dS and consumes a single output. Caller separately advances absolute
// time by context.output_time_step, not by recomputing n/Fo or endpoint/count.
bool advance_source_time_control(SourceTimeControlState& state,const char*& error) noexcept;
enum class SourceTimeAfterOutputAction { keep, freeze_source_refresh };
struct SourceTimeRenderContext {
    SourceTimeContext time;
    int source_refresh_budget = 0; // MUST equal min(139,blockCount) at admitted Fo48k
    int rate_external_budget = 0; // explicit additional caller R bound
    bool admit_pre_startup_source_refresh = false; // explicit bounded bootstrap
    // Optional read-only observation seam; never needed for rendering or fitting.
    void (*observe_before_output)(std::size_t,const PeriodClockState&,void*) = nullptr;
    void* observation_context = nullptr;
    // Optional bounded action AFTER own S/R advances and absolute-time advance.
    // Inputs are read-only. Only permitted mutation is remaining=INTMAX when
    // explicitly requested; NULL is bit-for-bit the original composition.
    SourceTimeAfterOutputAction (*after_output_action)(std::size_t,
        const SourceTimeControlState&,const PeriodClockState&,void*) = nullptr;
    void* after_output_context = nullptr;
};
struct SourceTimeControlEvent {
    std::size_t output_index = 0;
    bool bootstrap = false;
    double element_time = 0;
    double source_before = 0;
    double endpoint = 0;
    int returned_count = 0;
    int cached_index = 0;
    double source_step = 0;
};
struct SourceTimeRenderResult {
    bool ok = false;
    std::string error;
    PeriodRendererResult renderer;
    SourceTimeControlState final_source_state;
    double final_caller_time = 0;
    std::size_t rate_refreshes = 0;
    std::vector<SourceTimeControlEvent> source_events;
};
// Bounded independent composition via the existing renderer hook. Fo48k/D1,
// one explicitly supplied caller block, zero source offset, globalPitch1,
// identity source-time knots, Q STILL explicit constant p.probe_ratio. Scalar
// p.pitch_ratio/initial_source_position are superseded by supplied curves/time.
// The caller must acknowledge the initial positive-dS bootstrap explicitly.
// This is not a project parser or a claim of original exporter endpoint ownership.
SourceTimeRenderResult render_period_component_source_time(const std::vector<float>& pcm,
    const SampledFunctionView& periods,const SampledFunctionView& playback,
    IdentitySourceTimeView source_time,const PeriodRendererParameters& parameters,
    const SourceTimeRenderContext& caller);
} // namespace melodyne_re
