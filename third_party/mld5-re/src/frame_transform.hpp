#pragma once
#include "primitives.hpp"
#include <vector>

namespace melodyne_re {
struct FrameTransformParameters {
    int left_length = 0;  // a12
    int right_length = 0; // a13; occupied input prefix has left+right samples
    float ratio = 1.0f;   // a6, promoted to double before position calculations
    bool do_fft = false; // native a2; no-FFT entry rejects true; post-IFFT requires true
    bool do_resample = false; // native a4
    bool cubic = true;        // native this+609, false selects linear
    bool left_window_requested = false;  // *a20
    bool right_window_requested = false; // *a21
    bool left_window_done = false;       // *a22
    bool right_window_done = false;      // *a23
};
class FrameWindowTables {
public:
    explicit FrameWindowTables(const RaisedCosineKernel& kernel);
    const RaisedCosineKernel::Table& hann() const noexcept { return hann_; }
    const RaisedCosineKernel::Table& sqrt_hann() const noexcept { return sqrt_hann_; }
private:
    RaisedCosineKernel::Table hann_{};
    RaisedCosineKernel::Table sqrt_hann_{};
};
struct FrameTransformResult {
    bool ok = false;
    const char* error = nullptr;
    std::vector<std::vector<float>> channels; // native this+488, each buffer_size
    bool left_window_done = false;
    bool right_window_done = false;
    int resample_begin = 0; // diagnostics; original does not return these indices
    int resample_end = 0;   // exclusive
};
// Restricted 0x1819F9970 with doFFT=false and a8={nullptr,0}. No FFT,
// reference-phase work, component reconstruction, overlap-add or engine state.
// Samples are signed finite floats; channel buffers must have equal size>=2.
// No fast-math; compile -ffp-contract=off. See notes/frame-transform.md.
FrameTransformResult transform_frame_no_fft(
    const std::vector<std::vector<float>>& channels,
    const FrameTransformParameters& parameters,
    const FrameWindowTables& tables);
// Readout only after an ALREADY initial-windowed/FFT/inverse frame. Requires
// do_fft=true, never repeats the initial window, and always selects sqrt-Hann
// for final/short windows, even when do_resample=false. Does not perform FFT.
FrameTransformResult transform_frame_post_ifft(
    const std::vector<std::vector<float>>& channels,
    const FrameTransformParameters& parameters,
    const FrameWindowTables& tables);
} // namespace melodyne_re
