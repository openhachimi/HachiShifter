#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace melodyne_re {

// Float operation order recovered from 0x1819fab3a..0x1819fab8a.
// Compile with -ffp-contract=off and without fast-math.
float lagrange_cubic(float p0, float p1, float p2, float p3, float t) noexcept;

// Mode 0's float +/-0.5 followed by floor/ceil, saturating at int32 limits.
// Defensive NaN/zero/subnormal handling returns 0; see notes/primitives.md.
std::int32_t round_mode0(float value) noexcept;

class RaisedCosineKernel {
public:
    static constexpr std::size_t period = 16384;
    static constexpr std::size_t midpoint = 8192;
    using Table = std::array<float, period + 1>;

    RaisedCosineKernel();
    const Table& table() const noexcept { return table_; }
    // Quantized lookup, not continuous cosine evaluation or interpolation.
    float lookup(float x) const noexcept;

private:
    Table table_{};
};

} // namespace melodyne_re
