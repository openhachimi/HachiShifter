#pragma once
#include "frame_transform.hpp"
#include <cstdint>
#include <vector>

namespace melodyne_re {
struct SpectralFrameParameters {
    FrameTransformParameters frame;
    float source_hz = 100.0f; // native frame a14 -> component a12
    // Caller computes exactly float(float(Fs)/float(D)); NOT Fs/(D*NFFT)
    // in double. The processor divides this float by float(NFFT) for bin Hz.
    float processing_sample_rate = 32000.0f;
    float probe_ratio = 1.0f; // Q, DIRECT component a17; not inferred from frame.ratio
    float output_gain = 1.0f; // component a33, after normalization cap
    bool apply_component_mask = true;
    std::vector<float> harmonic_envelope; // optional exactly512 component a4 values
    bool reference_phase = false; // unsupported; false preserves source directions
    std::uint32_t unsupported_branches = 0; // other native shaper/object requests
    SpectralFrameParameters() { frame.do_fft=true; }
};
struct SpectralFrameResult {
    bool ok=false;
    const char* error=nullptr;
    std::vector<float> samples; // full original buffer extent, not only NFFT
    int fft_length=0;
    std::vector<float> source_magnitude; // NFFT/2, bin0=0
    std::vector<float> mask;             // NFFT/2, prior ones
    std::vector<float> target_magnitude; // empty if apply_component_mask=false
    std::vector<float> packed_spectrum;  // NFFT floats, after mask; endpoints paired
    float component_normalization=1.0f;
    bool left_window_done=false;
    bool right_window_done=false;
    int resample_begin=0;
    int resample_end=0;
};
// Restricted MONO 9970 composition: initial sqrt-Hann -> mathematical native-
// convention FFT -> stable magnitude -> optional admitted component mask ->
// inverse -> post-IFFT readout. No source/caller object resolution, reference
// phase, FFT-bit-parity claim, frame scheduling or overlap-add. buffer must hold
// both L+R and selected NFFT=min4096(max16(next_power_of_two(L+R))).
SpectralFrameResult process_spectral_frame_mono(
    const std::vector<float>& buffer,const SpectralFrameParameters& parameters,
    const FrameWindowTables& tables,const RaisedCosineKernel& kernel);
} // namespace melodyne_re
