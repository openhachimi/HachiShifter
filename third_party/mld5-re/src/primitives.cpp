#include "primitives.hpp"

#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "Recovered DSP arithmetic must not use fast-math"
#endif

namespace melodyne_re {

float lagrange_cubic(float p0, float p1, float p2, float p3, float t) noexcept {
    // Each assignment corresponds to one scalar SSE instruction; do not
    // reassociate even algebraically equivalent expressions.
    float xmm2 = p3 - p0;
    xmm2 = xmm2 * 0x1.555556p-3f;
    float xmm1 = p1 - p2;
    xmm1 = xmm1 * 0.5f;
    float xmm5 = p2 + p0;
    xmm5 = xmm5 * 0.5f;
    float xmm3 = xmm1 + xmm2;
    const float xmm0 = xmm5 - p1;
    xmm5 = xmm5 - xmm2;
    xmm5 = xmm5 - xmm1;
    xmm5 = xmm5 - p0;
    xmm3 = xmm3 * t;
    xmm3 = xmm3 + xmm0;
    xmm3 = xmm3 * t;
    xmm3 = xmm3 + xmm5;
    xmm3 = xmm3 * t;
    xmm3 = xmm3 + p1;
    return xmm3;
}

std::int32_t round_mode0(float value) noexcept {
    const int classification = std::fpclassify(value);
    if (classification == FP_NAN || classification == FP_ZERO ||
        classification == FP_SUBNORMAL)
        return 0;
    if (value >= 2147483648.0f)
        return std::numeric_limits<std::int32_t>::max();
    if (value <= -2147483648.0f)
        return std::numeric_limits<std::int32_t>::min();
    if (value >= 0.0f) {
        const float shifted = value + 0.5f;
        return static_cast<std::int32_t>(std::floor(shifted));
    }
    const float shifted = value - 0.5f;
    return static_cast<std::int32_t>(std::ceil(shifted));
}

RaisedCosineKernel::RaisedCosineKernel() {
    double phase = -3.141592653589793;
    constexpr double step = 0.0003834951969714103;
    for (std::size_t i = 0; i < period; ++i) {
        const double cosine = std::cos(phase);
        phase = phase + step; // Repeated accumulation, NOT -pi + i*step.
        const double shifted = cosine + 1.0;
        const double scaled = shifted * 0.5;
        table_[i] = static_cast<float>(scaled);
    }
    table_[period] = 0.0f;
}

float RaisedCosineKernel::lookup(float x) const noexcept {
    const float scaled = x * 8192.0f;
    const std::int32_t index = round_mode0(scaled);
    if (index < 0)
        return 0.0f;
    if (index > static_cast<std::int32_t>(midpoint))
        return 1.0f;
    return table_[static_cast<std::size_t>(index)];
}

} // namespace melodyne_re
