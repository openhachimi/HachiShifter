#pragma once
#include <vector>

namespace melodyne_re {
// Explicit snapshot of native 181E1CAA0 inputs. Offsets are observed; names
// describe the evaluator role, not a claim about serialized object loading.
struct PreparationEnvelopeSnapshot {
    float start = 1.0f;          // native envelope+68
    float rise_duration = 0.0f;  // +72, seconds
    float rise_end = 1.0f;       // +76
    float decay_start = 0.0f;    // +80, seconds
    float decay_factor = 1.0f;   // +88, pow base, per millisecond
    float end = 1.0f;            // +92
};
struct PreparationEnvelopeResult {
    bool ok=false;
    const char* error=nullptr;
    float value=0.0f;
};
// Ordered scalar port of 0x181E1CAA0 in a finite/nonnegative domain.
// This is NOT a full 0050 preparation/object resolver or shaper policy.
PreparationEnvelopeResult evaluate_preparation_envelope(
    const PreparationEnvelopeSnapshot& envelope,float elapsed_seconds);

// Restricted loaded neutral harmonic spectrum, native types0/1/2 only.
// Every control is explicit so a nonneutral request fails rather than silently
// becoming identity. EQtype3 and its frequency mapping are not implemented.
struct PreparationNeutralSpectrum {
    int spectrum_type=0;
    std::vector<float> base_target;
    std::vector<float> reference;
    std::vector<float> unreduced_factors;
    std::vector<float> formant_factors;
    float one_by_f_tilt_pow=1.0f;
    float one_by_f_diff_pow=0.0f;
    float comb_start=1.0f;
    float comb_balance=0.0f;
    float comb_width=2.0f;
};
struct PreparationSpectrumResult {
    bool ok=false;
    const char* error=nullptr;
    std::vector<float> applied_factors; // native Spectrum+112
    bool needs_process_spectrum=false; // native Spectrum+72, fixed .999..1.001 band
};
// Port of the neutral harmonic subgraph of181E1D7F0/20D70/20100/1DAC0.
// Host powf last bits are NOT claimed identical to original Windows CRT.
// Does not guess needsProcessFormants(+73) or ParameterSet derived flags.
PreparationSpectrumResult derive_neutral_harmonic_factors(
    const PreparationNeutralSpectrum& spectrum);

// Recomputed derived predicates from1D260/238A0/1DC8960. These are pure
// writer bodies only, not ownership/version resolution or notification caches.
bool preparation_envelope_needs_process(const PreparationEnvelopeSnapshot& envelope,
                                        bool is_bypassed) noexcept;
bool preparation_formants_need_process(const std::vector<float>& factors) noexcept;
struct PreparationSetPolicyInput {
    bool can_use=true,is_bypassed=false;
    bool common_spectrum=false,lower_spectrum=false,upper_spectrum=false;
    bool common_formant=false,lower_formant=false,upper_formant=false,equalizer_formant=false;
    bool equalizer=false;
    bool amplitude_envelope=false,difference_envelope=false,formant_envelope=false;
    float synth_amplitudes=0.0f,synth_phases=0.0f,amplitude_difference_power=1.0f,emphasize_power=0.0f;
};
struct PreparationSetPolicy {
    bool process_spectrum_shaper=false;
    bool process_lower_spectrum=false;
    bool process_upper_spectrum=false;
};
PreparationSetPolicy derive_preparation_set_policy(const PreparationSetPolicyInput& input) noexcept;
} // namespace melodyne_re
