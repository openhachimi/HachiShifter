#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace melodyne_re {
// Transcendental implementations are explicit so native oracle math can be used
// without silently assuming host-libm last-bit equivalence.
struct PitchCacheMath {
    float (*log_float)(float) = nullptr;
    float (*pow_float)(float,float) = nullptr;
    float (*exp_float)(float) = nullptr;
    double (*log_double)(double) = nullptr;
};
struct PitchCacheParameters {
    double source_sample_rate = 32000;
    double control_resolution = 441;
    double source_duration = .576;
    float source_pitch_center = 7118;
    float element_pitch_center = 7118;
    float drift_factor = 1;
    float modulation_factor = 1;
    bool has_join = false;
};
struct PitchCacheResult {
    bool ok = false;
    std::string error;
    std::vector<float> source_cents;
    std::vector<float> drift_smooth;
    std::vector<float> modulation_smooth;
    std::vector<float> display_pitch;
    std::vector<float> playback_ratio;
    double resolution = 0;
};
// Native181BBE560 restricted mode1, a8=1, precomputed coefficient. Float
// recurrence round order preserved. extent=max(1,trunc(-2.5/log(double(c)))).
// The math callback defaults to host std::log, not a native-bit-parity claim.
bool smooth_pitch_cache_mode1(std::vector<float>& values,float coefficient,
    std::string& error,double (*log_double)(double) = nullptr);
// Restricted181AF0FB0: positive finite periods, no joins, factors1 only;
// amplitude may be empty (native absent envelope) or have the same count.
// Keeps smoothing and BOTH subtract/add transforms even at factors1.
// Rejects outside the admitted interior pitch range rather than inventing clamp
// constants or implementing other builder branches. Compile without fast-math.
PitchCacheResult build_pitch_cache_unit_factors(const std::vector<float>& periods,
    const std::vector<float>& amplitude,const PitchCacheParameters& parameters,
    const PitchCacheMath& math = {});
} // namespace melodyne_re
