#pragma once
#include "sampled_function.hpp"
#include <cstddef>

namespace melodyne_re {
struct CachedRateParameters {
    double source_sample_rate = 32000;
    double output_sample_rate = 48000;
    int source_stride = 1; // this admitted controller is identity-time D1
    float global_pitch = 1; // global1 only, but native float multiply retained
};
struct CachedRateState {
    bool initialized = false;
    double rate = 0;
    double delta = 0;
    float initial_pitch = 1;
    int remaining = 0;
    int cached_index = 0;
    std::size_t refreshes = 0;
};
struct CachedRateRefresh {
    bool ok = false;
    const char* error = nullptr;
    double accessor_endpoint = 0;
    float endpoint_pitch = 0;
    double endpoint_rate = 0;
    int returned_count = 0;
    int returned_index = 0;
};
// Initializes current R from860, double->float then float*global1 then double
// promotion. Leaves remaining0 so caller must refresh before its first output.
bool initialize_cached_rate(const SampledFunctionView& playback,double source_seconds,
    const CachedRateParameters& parameters,CachedRateState& state,
    const char*& error) noexcept;
// Caller supplies actual positive source-seconds/output step and explicit
// request budget. Native returned count is used literally, not "repaired" to
// equal the endpoint's geometric step count. Refresh requires remaining==0.
CachedRateRefresh refresh_cached_rate(const SampledFunctionView& playback,
    double source_seconds,double source_step_seconds,int requested_count,
    const CachedRateParameters& parameters,CachedRateState& state) noexcept;
// Advances R BEFORE the renderer advances C, and consumes one output budget.
bool advance_cached_rate(CachedRateState& state,const char*& error) noexcept;
} // namespace melodyne_re
