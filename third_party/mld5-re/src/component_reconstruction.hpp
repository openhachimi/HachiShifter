#pragma once

#include "primitives.hpp"
#include <cstdint>
#include <vector>

namespace melodyne_re {

// This is the magnitude-to-mask stage at 0x181A04830, NOT an FFT, frequency
// translation, phase reconstruction, time-domain renderer or HachiShifter.
struct ComponentReconstructionParameters {
    float spacing_numerator = 1.0f;   // native a12
    float spacing_denominator = 1.0f; // native a22; d = a12/a22, in bins
    float envelope_probe_ratio = 1.0f;// native a17; probes source, never moves taps
    float output_gain = 1.0f;         // native a33, applied AFTER normalization

    // Explicit restricted-branch contract. Any nonneutral request is rejected.
    float blend_bias = 0.0f;          // a15
    float channel_shape_amount = 0.0f;// a24 (requires a27 in the original)
    float redistribution_amount = 0.0f;// a26 (requires a30 in the original)
    float energy_exponent = 1.0f;     // a34
    float harmonic_tilt = 1.0f;       // a36
    float envelope_exponent = 1.0f;   // a37
    float output_cap = 1.0f;          // a38
    // a5/a6, a18..a21, a27..a32 and this+168 analysis sinks are not implemented.
    // Set this if any such branch is requested; the call fails, not approximates.
    std::uint32_t unsupported_branches = 0;
};

struct ComponentReconstructionResult {
    bool ok = false;
    const char* error = nullptr;
    std::vector<float> target_magnitude; // native a7, before normalization
    std::vector<float> mask;             // native a3, final normalized ratio
    float source_sum = 0.0f;             // sums exclude bin 0
    float target_sum = 0.0f;
    float normalization = 1.0f;          // min(100, source_sum/target_sum)
    int last_component = 0;              // min(1023, ceil(N/d)), inclusive
};

// source_magnitude: native a2, N plain magnitudes (not squared or raised).
// previous_mask: native a3 on entry; required, as native tiny-source handling
// can preserve its value before output_gain/normalization multiplication.
// harmonic_envelope: empty => native a4=null; otherwise exactly 512 values,
// using envelope[min(h,511)]. No a5/a6 neighbor blending is implemented.
// Inputs must be finite and nonnegative. Rejections return empty outputs.
// Compile without fast-math, with -ffp-contract=off; see parameter/FP limits
// and native evidence in notes/component-reconstruction.md.
ComponentReconstructionResult reconstruct_component_mask(
    const std::vector<float>& source_magnitude,
    const std::vector<float>& previous_mask,
    const ComponentReconstructionParameters& parameters,
    const RaisedCosineKernel& kernel,
    const std::vector<float>& harmonic_envelope = {});

} // namespace melodyne_re
