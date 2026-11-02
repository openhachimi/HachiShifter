#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace melodyne_re {
// Conditional specialization of 1819FB2E0 for a PROVEN MUAudioSourceSibilantItem.
// No detector/noise generator, FFT/history, fitted gain, or inferred root mix.
struct UnpitchedSibilantParameters {
    double source_sample_rate=32000;
    double output_sample_rate=48000;
    double initial_source_position=0;
    std::size_t output_count=0;
    int history_span=0; // actual this+544 = B*selected renderer field68; required
    bool source_type_is_sibilant=false;
    bool identity_time_mapping=false;
    bool spectrum_shaper_absent=false;
    bool neutral_shaper_gain_verified=false; // admits present shaper only when allGain196==1 proven
    bool gain_curve_absent=false;
    // Normal controls are per-note edits. These are deliberately ignored only
    // after all sibilant/type/admission checks pass; generator controls are not.
    double local_note_pitch_ratio=1;
    double global_pitch_ratio=1;
    double global_formant_ratio=1;
    float source_gain_db=0;
    float sibilant_balance=0;
    // Required only for PARTIALLY straddling incremental reads. Wholly past EOF
    // has shortened count0, so EFCC0 is a no-op under either predicate. Raw
    // startup uses source-reader zero padding, not EFCC0.
    bool native_tail_helper_disabled=false;
    bool native_tail_fade_enabled=false; // explicit caller predicate for partially straddling reads
};
struct UnpitchedRefillEvent {
    std::size_t output_index=0;
    std::int64_t old_origin=0,new_origin=0,fetch_origin=0;
    int source_hops=0,fetch_offset=0,fetch_count=0;
    int tail_fade_begin=0,tail_fade_count=0,tail_zero_count=0;
    double source_position=0,read_position_before_rollover=0;
};
struct UnpitchedSibilantResult {
    bool ok=false;
    const char* error=nullptr;
    std::size_t rejected_output_index=0;
    std::vector<float> samples;
    std::vector<UnpitchedRefillEvent> events;
    double final_source_position=0,final_read_position=0;
    std::int64_t final_source_origin=0;
    double effective_pitch_ratio=1;
};
UnpitchedSibilantResult render_unpitched_sibilant_mono(
    const std::vector<float>& actual_sibilant_pcm,
    const UnpitchedSibilantParameters& parameters);
} // namespace melodyne_re
