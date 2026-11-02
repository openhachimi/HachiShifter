#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace melodyne_re {
// Restricted ordinary this+608 path. No detector, 632 bridge, FFT, or OLA DSP.
struct PeriodClockState {
    double source_position = 0; // *(*(this+264)+16), original source samples
    double source_step = 1;     // same smoother +24, per output sample
    double rate = 1;            // *(*(this+272)+16), grid samples/output sample
    double rate_delta = 0;      // same smoother +24
    double read_position = 0;   // this+528, rolling synthesis-grid coordinate
    double threshold = 1;      // this+552
    int previous_span = 1;     // this+536; amount to shift before frame generation
    int current_span = 1;      // this+540; next frame left/anchor span
    std::int64_t source_origin = 0; // this+560
    double previous_period = 1; // this+568, saved initial period per frame event
    bool source_reset = false; // this+321
    bool previous_right_window_done = false; // this+617
};
struct PeriodClockParameters {
    double source_sample_rate = 48000;
    double output_sample_rate = 48000;
    int source_stride = 1; // D, admitted source-fetch values 1/2/4
    int buffer_capacity = 4096;
    std::int64_t component_source_start = 0;
    // Original block's direction flag, not inferred from an individual source_step.
    bool forward = true;
    // v1206: if a nonunit hop count would enter 632, reject instead of approximating.
    bool bridge_on_nonunit_hop = false;
    bool do_fft = false;
    bool do_resample = false;
    // Safety budget only. Exceeding it returns unsupported; native has no such cap.
    std::size_t correction_budget = 100000;
};
// Source period in ORIGINAL SOURCE SAMPLES, evaluated at component-relative
// seconds. Callback must supply project/externally specified P, never a detector.
using SourcePeriod = std::function<float(double)>;

struct PeriodFramePlan {
    bool ok = false;
    bool due = false;
    const char* error = nullptr;
    int source_hop_count = 1; // native v618
    std::size_t period_queries = 0;
    float initial_period = 0;
    float final_period = 0;   // actual float a16 to 1819F9970
    float source_hz = 0;      // based on INITIAL current and previous P
    float target_ratio = 0;   // rate/(Fs/D/Fo), evaluated before frame changes
    double final_period_query_seconds = 0;
    double predicted_source_position = 0;
    std::int64_t selected_source_origin = 0;
    int shift_samples = 0;    // shift old rolling buffer by previous_span first
    int left_length = 0;
    int right_length = 0;
    bool full_rebuild = false;
    std::int64_t fetch_source_origin = 0; // before MUAudioFileSource origin correction
    int fetch_destination_offset = 0;
    int fetch_count = 0;      // processing-grid samples
    bool new_left_window_requested = false;
    bool new_right_window_requested = false;
    bool old_left_window_requested = false;
    // Normal non-632 placement: raw full ADD L/COPY H; processed ADD L+H/no COPY.
    // Raw incremental skips ADD. Right-window request identifies processed support.
    int add_prefix_count = 0;
    int copy_tail_offset = 0;
    int copy_tail_count = 0;
    PeriodClockState next_state; // after frame clock rollover, before output emission
};
PeriodFramePlan plan_period_frame(const PeriodClockState& state,
                                 const PeriodClockParameters& parameters,
                                 const SourcePeriod& period);
// Attach the frame processor's actual *a23 result before accepting next_state.
void set_period_frame_right_window_done(PeriodFramePlan& plan, bool done) noexcept;
// Exact low-quality linear rolling-buffer read branch; no gain/channel mixing.
// Returns false for missing guards/unsupported negative coordinates.
bool read_period_linear(const std::vector<float>& rolling, double position,
                        float& output) noexcept;
// 181A02AE4..181A02B24: increment S; update rate first; then C += UPDATED rate.
// Updates only represented clocks; external Q/gain/time ramps remain caller-owned.
bool advance_period_output_clock(PeriodClockState& state) noexcept;
// 1819F34A0 sets BOTH processing/rolling capacities from output rate. Invalid=>0.
int native_period_buffer_capacity(double output_sample_rate) noexcept;
// 1819F5560 startup uses DOUBLE period curve values, unlike normal frame's float P.
using StartupSourcePeriod = std::function<double(double)>;
struct PeriodStartupParameters {
    double source_sample_rate = 48000;
    int source_stride = 1;
    int processing_capacity = 0; // MUST supply actual this+356, no guessed native default
    int rolling_capacity = 0;    // MUST supply actual this+352
    double source_position = 0;
    std::int64_t component_source_start = 0;
    // Refreshed per-output control values: native invalidates ramps AFTER startup.
    double source_step = 1;
    double rate = 1;
    double rate_delta = 0;
    float q = 1; // actual Q from local/global formant and pitch-exponent helper
    float target_pitch_ratio = 1; // startup a15 to 1819F9970
    bool reference_enabled = false; // true explicitly unsupported until ctor phase init known
    bool source_reset = false; // preserve explicit incoming this+321; startup doesn't clear it
    bool end_taper_active = false; // explicit *(int*)(*(this+248)+16)==0 condition
    std::int64_t component_source_end = 0;
};
struct PeriodStartupFrame {
    double period_query_seconds = 0;
    double source_period = 0;
    std::int64_t fetch_source_origin = 0;
    int fetch_count = 0;
    int left_length = 0;
    int right_length = 0;
    std::int64_t component_source_position = 0; // 9970 a9, differs from fetch origin
    float source_hz = 0;
    int fft_size = 0; // 9970 chooses next power of two >=L+R, min16, cap4096
    float bin_hz = 0; // float(float(Fs)/float(D))/float(N)
    float float_period = 0;
    float target_pitch_ratio = 1;
    float q = 1;
    float rho = 1; // fully allocated normal branch only
    bool do_fft = true;
    bool left_window_requested = true;
    bool right_window_requested = true;
    int add_destination_offset = 0; // may be -1: rolling buffer MUST have left guard
    int add_source_offset = 0;
    int add_count = 0;
};
struct PeriodStartupPlan {
    bool ok = false;
    const char* error = nullptr;
    bool cubic = true; // unconditionally set this+609 at 1819FB6A4
    bool bridge_632 = false; // caller clears at 1819FDD92
    float startup_q = 1; // min(Q,1)
    std::vector<double> period_queries;
    // Raw branch fetches into rolling[-1]; process branch emits three frame events.
    bool raw_prefetch = false;
    std::int64_t raw_fetch_source_origin = 0;
    int raw_fetch_destination_offset = -1;
    int raw_fetch_count = 0;
    int initial_clear_begin = -1;
    int initial_clear_count = 0;
    int tail_clear_begin = 0;
    int tail_clear_count = 0;
    int end_taper_begin = 0; // raw branch only; initial amplitude1, multiply ratio after each sample
    int end_taper_count = 0;
    float end_taper_ratio = 1;
    std::vector<PeriodStartupFrame> frames;
    PeriodClockState state; // valid only AFTER caller executes all prefill events
};
// Restricted startup: optional callback absent means native Fs/100 fallback.
// No shaping DSP or sample writes; allocation failure/reference startup unsupported.
PeriodStartupPlan plan_period_startup(const PeriodStartupParameters& parameters,
                                     const StartupSourcePeriod& period = {});
} // namespace melodyne_re
