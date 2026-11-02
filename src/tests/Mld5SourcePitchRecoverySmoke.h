#pragma once

#include "../backend/Mld5SourcePitchRecovery.h"
#include <juce_core/juce_core.h>
#include <cstdint>
#include <iostream>
#include <limits>

namespace hachi
{
// Analytic PCM and explicit caller permissions; no models, project import,
// mutable source files, audio devices or claimed results from an unrun build.
inline bool runMld5SourcePitchRecoverySmoke(const juce::File& out)
{
    using Recovery = backend::Mld5SourcePitchRecovery;
    constexpr double step = .005;
    constexpr double pi2 = 6.283185307179586476925286766559;
    const auto midi = [](double hz) { return 69 + 12 * std::log2(hz / 440); };
    const auto identity = [](double t) { return t; };
    const auto zero = [](const std::vector<float>& values)
    { return std::all_of(values.begin(), values.end(), [](float v) { return v == 0; }); };
    const auto tone = [=](double rate, double seconds, double hz, double amplitude = .2)
    {
        std::vector<float> pcm(std::size_t(std::llround(rate * seconds)));
        for (std::size_t i = 0; i < pcm.size(); ++i)
            pcm[i] = float(amplitude * std::sin(pi2 * hz * double(i) / rate));
        return pcm;
    };
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const juce::String& name, bool passed)
    {
        ok &= passed;
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("passed", passed); checks.add(row);
        std::cout << name << '=' << (passed ? "PASS" : "FAIL") << std::endl;
    };
    auto pcm = tone(44100, 1, 420);
    const auto unchangedPcm = pcm;
    std::vector<float> source(201, float(midi(420))), target(201, 79);
    std::vector<float> hint(201, 68.2f), intent(201, 79);
    std::size_t wanted = 0;
    for (std::size_t i = 0; i < source.size(); ++i)
        if ((i >= 60 && i <= 90) || (i >= 150 && i <= 199))
        { source[i] = target[i] = 0; ++wanted; }
    const auto beforeSource = source, beforeTarget = target;
    int mapped = 0;
    const auto rows = Recovery::recover(pcm, 44100, step,
        [&](double t) { ++mapped; return t; }, hint, intent, source, target);
    bool repaired = rows.size() == wanted, knownUntouched = true, reportExact = true;
    for (const auto& row : rows)
    {
        repaired &= std::abs(row.sourceMidi - midi(420)) < .02 && row.targetMidi == 79
            && row.confidence >= .97f && source[row.index] == row.sourceMidi
            && target[row.index] == row.targetMidi;
        reportExact &= std::abs(row.sourceSeconds - double(row.index) * step) < 1.e-12;
    }
    for (std::size_t i = 0; i < source.size(); ++i)
        if (beforeSource[i] != 0 || beforeTarget[i] != 0)
            knownUntouched &= source[i] == beforeSource[i] && target[i] == beforeTarget[i];
    check("420Hz_mid_and_physical_tail_gaps_measured_source_target79", repaired);
    check("valid_source_and_target_are_bitwise_untouched", knownUntouched);
    check("one_source_mapping_per_eligible_frame_report_keeps_original_clock",
          mapped == int(wanted) && reportExact);
    check("input_PCM_is_read_only", pcm == unchangedPcm);

    // A hint narrows the search, but is never treated as the actual F0.
    auto measured = tone(44100, 1, 440);
    source.assign(1, 0); target = source;
    const auto different = Recovery::recover(measured, 44100, step, [](double) { return .5; },
        {68.2f}, {79}, source, target);
    check("real_440Hz_overrides_420Hz_hint_without_moving_target",
          different.size() == 1 && std::abs(source[0] - 69) < .02 && target[0] == 79);

    // Native sample rate and fractional lag, not a hardcoded internal rate.
    for (const auto rate : {32000.0, 44100.0, 48000.0})
    {
        measured = tone(rate, 1, 422.37);
        source.assign(201, 0); target = source; hint.assign(201, 68.2f); intent.assign(201, 79);
        const auto scan = Recovery::recover(measured, rate, step, identity, hint, intent, source, target);
        bool exact = scan.size() == 200; // t=1 is outside PCM; no padded/extrapolated sample
        for (const auto& row : scan)
            exact &= row.index < 200 && std::abs(row.sourceMidi - midi(422.37)) < .02
                && row.targetMidi == 79 && std::isfinite(row.confidence);
        check("native_rate_fractional_lag_one_second_scan_" + juce::String(int(rate)), exact);
    }

    // Chirp instantaneous F0 = 420+40*t in the SOURCE clock; a nonlinear map
    // distinguishes one mapping from accidental identity or repeated mapping.
    pcm.resize(48000);
    for (std::size_t i = 0; i < pcm.size(); ++i)
    {
        const auto t = double(i) / 48000;
        pcm[i] = float(.2 * std::sin(pi2 * (420 * t + 20 * t * t)));
    }
    source.assign(201, 0); target = source; hint.assign(201, 68.2f); intent.assign(201, 79);
    const auto map = [](double t) { return .1 + .6 * t + .1 * t * t; };
    mapped = 0;
    const auto warped = Recovery::recover(pcm, 48000, step,
        [&](double t) { ++mapped; return map(t); }, hint, intent, source, target);
    bool sourceClock = warped.size() == source.size() && mapped == int(source.size());
    for (const auto& row : warped)
        sourceClock &= std::abs(row.sourceSeconds - map(double(row.index) * step)) < 1.e-12
            && std::abs(row.sourceMidi - midi(420 + 40 * row.sourceSeconds)) < .03
            && row.targetMidi == 79;
    check("nonidentity_nonlinear_source_clock_is_mapped_once", sourceClock);

    const auto rejected = [&](const juce::String& name, const std::vector<float>& audio,
                              double rate, float permission = 68.2f, float targetPermission = 79)
    {
        std::vector<float> s(201, 0), t(201, 0), h(201, permission), in(201, targetPermission);
        const auto found = Recovery::recover(audio, rate, step, identity, h, in, s, t);
        check(name, found.empty() && zero(s) && zero(t));
    };
    std::uint32_t seed = 0x12345678;
    std::vector<float> noise(44100), frication(44100), mixed(44100);
    float previous = 0;
    for (std::size_t i = 0; i < noise.size(); ++i)
    {
        seed = 1664525u * seed + 1013904223u;
        const auto white = float(double(seed) / 4294967296.0 * 2 - 1);
        noise[i] = .2f * white; frication[i] = .15f * (white - previous); previous = white;
        mixed[i] = unchangedPcm[i] + noise[i];
    }
    rejected("white_noise_not_recovered", noise, 44100);
    rejected("fricative_highpass_noise_not_recovered", frication, 44100);
    rejected("noisy_weak_periodicity_not_recovered", mixed, 44100);
    rejected("weak_periodic_PCM_below_energy_gate_not_recovered", tone(44100, 1, 420, 1.e-5), 44100);
    rejected("true_silence_not_recovered", std::vector<float>(44100, 0), 44100);
    rejected("DC_is_not_periodicity", std::vector<float>(44100, .2f), 44100);
    rejected("allUV_no_independent_hint_stays_unpitched", unchangedPcm, 44100, 0);
    rejected("explicit_no_target_intent_stays_unpitched", unchangedPcm, 44100, 68.2f, 0);
    rejected("short_buffer_cannot_observe_full_window", tone(44100, .01, 420), 44100);
    rejected("low_F0_buffer_with_less_than_four_periods_rejected", tone(44100, .04, 60), 44100, float(midi(60)));
    rejected("octave_wrong_hint_does_not_relabel_840_as_420", tone(44100, 1, 840), 44100);
    rejected("pitch_outside_hint_range_not_guessed", tone(44100, 1, 300), 44100);

    // Real-case-shaped physical EOF: legacy missing F0 .3599..38639 s.
    pcm = tone(44100, .38639, 420);
    source.assign(6, 0); target = source; hint.assign(6, 68.2f); intent.assign(6, 79);
    const auto tailMap = [](double t) { return .3599 + t; };
    const auto tail = Recovery::recover(pcm, 44100, step, tailMap, hint, intent, source, target);
    bool tailGood = tail.size() == 6;
    for (const auto& row : tail)
        tailGood &= std::abs(row.sourceMidi - midi(420)) < .02 && row.targetMidi == 79
            && std::abs(row.sourceSeconds - (.3599 + double(row.index) * step)) < 1.e-12;
    check("physical_EOF_vowel_last_26ms_recovers_without_clock_shift", tailGood);
    auto silentTail = pcm;
    for (std::size_t i = std::size_t(.36 * 44100); i < silentTail.size(); ++i) silentTail[i] = 0;
    source.assign(6, 0); target = source;
    const auto silent = Recovery::recover(silentTail, 44100, step,
        [](double t) { return .361 + t; }, hint, intent, source, target);
    check("last_25ms_silence_cannot_borrow_preceding_vowel", silent.empty() && zero(source) && zero(target));

    // Partial known pairs, malformed permission and all unsafe input classes.
    source = {68.2f, 0, 68.2f, 0}; target = {0, 79, 79, 0};
    const auto knownS = source, knownT = target;
    const auto partial = Recovery::recover(unchangedPcm, 44100, step, [](double) { return .5; },
        {68.2f, 68.2f, 68.2f, 0}, {79, 79, 79, 79}, source, target);
    check("partial_pairs_and_explicit_permission_zero_unchanged", partial.empty() && source == knownS && target == knownT);
    const auto nan = std::numeric_limits<double>::quiet_NaN();
    const auto inf = std::numeric_limits<double>::infinity();
    const auto badCall = [&](double rate, double seconds,
                             const std::function<double(double)>& mapping,
                             const std::vector<float>& h, const std::vector<float>& in)
    {
        std::vector<float> s(1, 0), t(1, 0);
        const auto r = Recovery::recover(unchangedPcm, rate, seconds, mapping, h, in, s, t);
        return r.empty() && zero(s) && zero(t);
    };
    bool invalid = true;
    for (const auto rate : {0.0, -1.0, nan, inf, 7999.0, 192001.0})
        invalid &= badCall(rate, step, identity, {68.2f}, {79});
    for (const auto dt : {0.0, -1.0, nan, inf, .0001, 2.0})
        invalid &= badCall(44100, dt, identity, {68.2f}, {79});
    for (const auto position : {-1.0, nan, inf, 1.0, 1.e100})
        invalid &= badCall(44100, step, [=](double) { return position; }, {68.2f}, {79});
    invalid &= badCall(44100, step, {}, {68.2f}, {79})
        && badCall(44100, step, identity, {}, {79})
        && badCall(44100, step, identity, {68.2f}, {})
        && badCall(44100, step, identity, {float(nan)}, {79})
        && badCall(44100, step, identity, {68.2f}, {float(inf)});
    check("NaN_infinity_invalid_rate_step_map_and_shape_rejected", invalid);
    measured = unchangedPcm; measured[22050] = float(nan);
    source.assign(1, 0); target = source;
    check("nonfinite_local_PCM_rejected", Recovery::recover(measured, 44100, step,
        [](double) { return .5; }, {68.2f}, {79}, source, target).empty() && zero(source) && zero(target));
    source.assign(Recovery::maxFrames + 1, 0); target = source;
    hint.assign(source.size(), 68.2f); intent.assign(source.size(), 79);
    check("frame_cap_rejects_without_partial_edits", Recovery::recover(unchangedPcm, 44100,
        step, identity, hint, intent, source, target).empty() && zero(source) && zero(target));

    source.assign(20, 0); target = source; hint.assign(20, 68.2f); intent.assign(20, 79);
    bool cancelled = false; int visited = 0;
    try
    {
        (void)Recovery::recover(unchangedPcm, 44100, step, [](double t) { return .2 + t; },
            hint, intent, source, target, [&] { if (++visited == 5) throw 1701; });
    }
    catch (int value) { cancelled = value == 1701; }
    check("cancellation_is_transactional_after_prior_candidates", cancelled && visited == 5 && zero(source) && zero(target));
    bool mapThrew = false; mapped = 0;
    try
    {
        (void)Recovery::recover(unchangedPcm, 44100, step,
            [&](double t) { if (++mapped == 4) throw 1702; return .2 + t; }, hint, intent, source, target);
    }
    catch (int value) { mapThrew = value == 1702; }
    check("throwing_source_mapping_is_transactional", mapThrew && zero(source) && zero(target));

    if (out.createDirectory().failed()) return false;
    auto* report = new juce::DynamicObject(); report->setProperty("passed", ok);
    report->setProperty("checks", checks); report->setProperty("new_models_added", false);
    report->setProperty("analytic_signals_only", true);
    return out.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report), true)) && ok;
}
}
