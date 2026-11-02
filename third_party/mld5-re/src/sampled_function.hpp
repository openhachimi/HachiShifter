#pragma once
#include <cstddef>

namespace melodyne_re {
// MUSampledFunction 181DAA860: values are GNData float32 payload, resolution is
// serialized this+80. The accessor itself assigns no period/pitch value units.
struct SampledFunctionView {
    const float* values = nullptr;
    std::size_t count = 0;
    double resolution = 1.0;
};
struct SampledFunctionResult {
    bool ok = false;
    double value = 0.0;
    const char* error = nullptr;
};
// Finite x, positive finite resolution when a segment is evaluated; IEEE binary64
// round-to-nearest, no fast-math. Rejects native index-conversion/overflow hazards.
// Each arithmetic operation rounds separately; this is not an FMA interpolator.
SampledFunctionResult evaluate_sampled_function(const SampledFunctionView& function,
                                               double x,
                                               bool extrapolate = false) noexcept;

struct SampledSegmentResult {
    bool ok=false;
    double value=0.0;
    int count=0;
    int cached_index=0;
    const char* error=nullptr;
};
// Bounded positive-dx 181DAA9D0, NOT the860 point accessor. Preserves native
// count/cache in/out semantics, including count increases at boundary cases.
// Empty/missing data returns0 and leaves both supplied integers unchanged.
// Requires>=2 samples otherwise; rejects zero/reverse steps and unsafe numeric
// conversion domains. Failure returns unchanged count/cache, not partial writes.
SampledSegmentResult evaluate_sampled_segment_forward(
    const SampledFunctionView& function,double x,double dx,
    int requested_count,int cached_index,bool extrapolate=false) noexcept;
} // namespace melodyne_re
