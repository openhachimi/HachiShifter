#pragma once
#include "../backend/Mld5NativeControl.h"
#include <juce_core/juce_core.h>
#include <iostream>
#include <limits>
#include <vector>

namespace hachi
{
// No external media or device. Arithmetic tests exercise the REAL recovered
// advance_period_output_clock; independent analytic-sine tests also render the
// recovered period core, without routing through Mld5Renderer or fitting phase.
// StretchController's smaller geometry API is NOT a complete DSP oracle.
// The test writes verification.txt when invoked; existence is not a run result.
inline bool runMld5NativeControlSmoke(const juce::File& out)
{
    using Control = backend::Mld5NativeControl;
    using Parameters = Control::Parameters;
    using Clock = melodyne_re::PeriodClockState;
    bool ok = true; juce::String log;
    const auto check = [&](const char* name, bool pass)
    {
        ok &= pass; log << name << "=" << (pass ? "PASS" : "FAIL") << "\n";
        std::cout << name << "=" << (pass ? "PASS" : "FAIL") << std::endl;
    };
    const auto near = [](double a, double b, double tolerance = 2.e-9)
    { return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= tolerance; };
    const auto fixture = [](int count)
    {
        Parameters p; p.duration = double(count) / p.outputRate;
        p.sourceAt = [](double t) { return t / 1.25; };
        return p;
    };
    const auto initialize = [](Control& c, Clock& s, const char*& error)
    {
        melodyne_re::PeriodStartupParameters startup;
        if (!c.initialize(startup, error)) return false;
        s.source_position = startup.source_position; s.source_step = startup.source_step;
        s.rate = startup.rate; s.rate_delta = startup.rate_delta;
        s.read_position = 7.25; s.threshold = 1.e9;
        return true;
    };
    const auto advance = [](Control& c, Clock& s, std::size_t index,
                            std::size_t remaining, const char*& error)
    {
        return c.before_output(s, index, remaining, error)
            && melodyne_re::advance_period_output_clock(s) && c.after_output(s, error);
    };
    const double base = 32000. / 48000.;
    const char* error = nullptr;

    {
        auto p = fixture(1000); Control c(p); Clock s;
        bool pass = initialize(c, s, error);
        check("explicit_pre_source_refresh_has_positive_dS_before_startup", pass
            && near(s.source_step, base / 1.25, 1.e-14) && s.source_position == 0.
            && s.rate == base && c.remaining() == 0 && c.refreshCount() == 0);
        const auto untouchedC = s.read_position;
        pass = pass && c.before_output(s, 0, 1000, error);
        check("first_before_refresh_does_not_reset_S_R_or_C", pass && s.source_position == 0.
            && s.rate == base && s.read_position == untouchedC && c.remaining() == 139);
        pass = pass && melodyne_re::advance_period_output_clock(s) && c.after_output(s, error);
        for (std::size_t n = 1; pass && n < 1000; ++n)
            pass = advance(c, s, n, 1000 - n, error) && near(s.source_step, base / 1.25, 1.e-12)
                && s.rate == base && s.rate_delta == 0.;
        check("T1_25_stretch_changes_source_step_not_neutral_R", pass
            && near(s.source_position, 1000. * base / 1.25) && c.nextOutputIndex() == 1000
            && near(s.read_position, untouchedC + 1000. * base));
        check("native_source_budget_persists_between_samples", pass && c.refreshCount() == 8 && c.remaining() == 0);
    }
    {
        auto p = fixture(500); const double pitch = std::exp2(5. / 12.);
        p.pitchRatioAt = [pitch](double) { return pitch; };
        Control c(p); Clock s; bool pass = initialize(c, s, error);
        const double expectedR = base * double(float(pitch));
        for (std::size_t n = 0; pass && n < 500; ++n)
            pass = advance(c, s, n, 500 - n, error) && s.rate == expectedR
                && near(s.source_step, base / 1.25, 1.e-12);
        check("plus_5_semitones_R_uses_native_float_pitch_and_Fs_over_Fo", pass && near(expectedR / base, pitch, 1.e-7));
        check("pitch_ratio_does_not_multiply_stretch_source_step", pass && near(s.source_position, 500. * base / 1.25));
    }
    {
        auto p = fixture(120); const double a = 17.25 / p.outputRate, b = 83. / p.outputRate;
        p.targetBoundaries = { 0., a, b, p.duration };
        p.sourceAt = [a, b](double t)
        {
            if (t <= a) return .8 * t;
            if (t <= b) return .8 * a + 1.2 * (t - a);
            return .8 * a + 1.2 * (b - a) + .5 * (t - b);
        };
        Control c(p); Clock s; bool pass = initialize(c, s, error);
        bool budgets = pass, endpoints = pass, continuity = pass; double previousS = s.source_position;
        for (std::size_t n = 0; pass && n < 120; ++n)
        {
            pass = c.before_output(s, n, 120 - n, error);
            continuity &= pass && s.source_position == previousS;
            if (n == 0) budgets &= c.remaining() == 17;
            if (n == 17) budgets &= c.remaining() == 1 && near(s.source_step, base * 1.1, 1.e-12);
            if (n == 18) budgets &= c.remaining() == 65;
            if (n == 83) budgets &= c.remaining() == 37;
            pass = pass && melodyne_re::advance_period_output_clock(s) && c.after_output(s, error);
            previousS = s.source_position;
            if (n == 16 || n == 17 || n == 82 || n == 119)
                endpoints &= near(s.source_position, p.sourceAt(double(n + 1) / p.outputRate) * p.sourceRate);
        }
        check("fractional_anchor_isolated_to_one_straddling_sample_not_rounded_key", pass && budgets
            && p.targetBoundaries[1] == a && near(a * p.outputRate, 17.25, 1.e-12));
        check("piecewise_source_endpoints_and_integral_boundary_stable", pass && endpoints && c.refreshCount() == 4);
        check("piecewise_refresh_never_reassigns_source_position", pass && continuity);
    }
    {
        auto p = fixture(100); p.sourceBudget = 20;
        const double duration = p.duration;
        p.pitchRatioAt = [duration](double t) { return 1. + .4 * t / duration; };
        p.probeRatioAt = [duration](double t) { return float(.8 + .3 * t / duration); };
        Control c(p); Clock s; bool pass = initialize(c, s, error);
        bool continuous = pass, updatedRead = pass, qClock = pass, endpoints = pass;
        double previousRate = s.rate;
        for (std::size_t n = 0; pass && n < 100; ++n)
        {
            const double oldRead = s.read_position;
            pass = c.before_output(s, n, 100 - n, error);
            continuous &= pass && s.rate == previousRate && s.rate_delta > 0.;
            qClock &= c.frame_probe_ratio(9.f) == p.probeRatioAt(double(n) / p.outputRate);
            if (n == 0) continuous &= near(s.rate_delta, (base * double(float(1.08)) - base) / double(float(20)), 1.e-15);
            pass = pass && melodyne_re::advance_period_output_clock(s) && c.after_output(s, error);
            updatedRead &= s.read_position == oldRead + s.rate;
            if ((n + 1) % 20 == 0)
                endpoints &= near(s.rate, base * double(float(p.pitchRatioAt(double(n + 1) / p.outputRate))), 2.e-14);
            previousRate = s.rate;
        }
        check("dynamic_pitch_has_continuous_endpoint_minus_current_over_float_count_ramps", pass && continuous && endpoints);
        check("rolling_C_advances_with_updated_not_previous_R", pass && updatedRead);
        check("dynamic_Q_uses_current_frame_support_target_clock_without_source_rewarp", pass && qClock);
        check("processed_frame_topology_is_optional_and_default_false", !c.requires_processed_frames());
        p.processedFrames = true; Control processed(p); Clock processedClock;
        const bool processedReady = initialize(processed, processedClock, error);
        check("processed_frame_topology_request_does_not_change_Q_math", processedReady
            && processed.requires_processed_frames() && processed.frame_probe_ratio(1.f) == p.probeRatioAt(0.));
    }
    {
        auto p = fixture(777); p.sourceBudget = 41;
        p.targetBoundaries = { 0., 123.5 / p.outputRate, 401. / p.outputRate, p.duration };
        p.pitchRatioAt = [](double t) { return 1. + 10. * t; };
        struct Snapshot { double source, step, rate, delta, read; int remaining; std::size_t refreshes; };
        const auto drive = [&](const std::vector<int>& chunks, std::vector<Snapshot>& history)
        {
            Control c(p); Clock s; const char* why = nullptr;
            if (!initialize(c, s, why)) return false;
            std::size_t n = 0, chunk = 0;
            while (n < 777)
            {
                const int count = std::min(chunks[chunk++ % chunks.size()], int(777 - n));
                for (int i = 0; i < count; ++i, ++n)
                {
                    if (!advance(c, s, n, std::size_t(count - i), why)) return false;
                    history.push_back({s.source_position, s.source_step, s.rate, s.rate_delta,
                                       s.read_position, c.remaining(), c.refreshCount()});
                }
            }
            return true;
        };
        std::vector<Snapshot> whole, split;
        bool pass = drive({777}, whole) && drive({1,7,13,64,3,127}, split) && whole.size() == split.size();
        for (std::size_t i = 0; pass && i < whole.size(); ++i)
        {
            const auto& a = whole[i]; const auto& b = split[i];
            pass = a.source == b.source && a.step == b.step && a.rate == b.rate && a.delta == b.delta
                && a.read == b.read && a.remaining == b.remaining && a.refreshes == b.refreshes;
        }
        check("chunk_boundaries_do_not_reset_or_rebudget_component_S_R_C", pass && whole.size() == 777);
    }
    {
        const auto rejected = [&](Parameters p)
        {
            Control c(std::move(p)); Clock s; const char* why = nullptr;
            return !initialize(c, s, why) && why && c.failureReason();
        };
        auto p = fixture(100); p.sourceRate = 0.; check("zero_source_rate_rejected_with_reason", rejected(p));
        p = fixture(100); p.outputRate = std::numeric_limits<double>::quiet_NaN();
        check("nonfinite_output_rate_rejected_with_reason", rejected(p));
        p = fixture(100); p.sourceAt = [](double t) { return 1. - t; };
        check("reverse_source_map_rejected_not_repaired", rejected(p));
        p = fixture(100); p.sourceAt = [](double) { return std::numeric_limits<double>::infinity(); };
        check("nonfinite_source_map_rejected", rejected(p));
        p = fixture(100); p.targetBoundaries = { .001, .0005 };
        check("unsorted_author_anchors_rejected_not_sorted", rejected(p));
        p = fixture(100); p.pitchRatioAt = [](double) { return -1.; };
        check("nonpositive_pitch_rate_rejected", rejected(p));
        p = fixture(100); p.probeRatioAt = [](double) { return 0.f; };
        check("invalid_Q_rejected_with_reason", rejected(p));
        p = fixture(100); p.cancelled = [] { return true; };
        check("cancel_callback_stops_before_source_startup", rejected(p));
        p = fixture(100); p.sourceBudget = 1; bool reverse = false;
        p.sourceAt = [&reverse](double t) { return reverse ? 0. : t + 1.; };
        Control c(p); Clock s; bool pass = initialize(c, s, error) && advance(c, s, 0, 100, error);
        reverse = true;
        check("map_reversal_during_refresh_fails_without_resetting_S", pass && !c.before_output(s, 1, 99, error)
            && error && s.source_position > 32000.);
    }
    {
        auto p = fixture(50); p.pitchRatioAt = [](double t) { return 1. + t * 100.; };
        Control c(p); Clock s; bool pass = initialize(c, s, error) && c.before_output(s, 0, 50, error);
        if (pass)
        {
            // Deliberately simulate the wrong old-R advance; do not invoke DSP.
            s.source_position += s.source_step; s.read_position += s.rate; s.rate += s.rate_delta;
        }
        check("after_output_rejects_C_advanced_with_stale_R", pass && !c.after_output(s, error) && error);
        Control rolled(p); Clock r; pass = initialize(rolled, r, error);
        r.read_position = 8.25; r.threshold = 8.;
        pass = pass && rolled.before_output(r, 0, 50, error);
        r.read_position -= r.threshold; // emulate actual library frame rollover
        pass = pass && melodyne_re::advance_period_output_clock(r) && rolled.after_output(r, error);
        check("after_output_allows_library_owned_single_threshold_rollover", pass);
        Control overflow(p); Clock o; pass = initialize(overflow, o, error)
            && overflow.before_output(o, 0, 50, error);
        o.source_position = std::numeric_limits<double>::infinity();
        check("after_output_rejects_clock_overflow_with_reason", pass && !overflow.after_output(o, error) && error);
    }
    {
        // Independent oracle: known PCM and exact integer source period, not an
        // F0 detector, a second Mld5Renderer render or a self-derived clock trace.
        // Expected phase is fixed BEFORE observing the output; no lag, gain,
        // phase, frequency or threshold fitting is allowed, even on failure.
        constexpr double rate = 48000., amplitude = .05, period = 192.;
        constexpr std::size_t count = 48000, sourceCount = 96000;
        const double omega = 2. * std::acos(-1.) / period;
        log << "independent_oracle.Fs=48000\nindependent_oracle.Fo=48000\nindependent_oracle.source_period=192"
               "\nindependent_oracle.source_samples=96000\nindependent_oracle.output_samples=48000"
               "\nindependent_oracle.amplitude=0.05\nindependent_oracle.max_abs_tolerance=0.0001"
               "\nindependent_oracle.dynamic_phase_samples=n+0.3*n*(n+1)/(2*48000)\n";
        std::vector<float> pcm(sourceCount), periods(402, float(period));
        for (std::size_t n = 0; n < pcm.size(); ++n)
            pcm[n] = float(amplitude * std::sin(omega * double(n)));
        const melodyne_re::SampledFunctionView sourcePeriods{periods.data(), periods.size(), 200.};
        melodyne_re::PeriodRendererParameters render;
        render.source_sample_rate = render.output_sample_rate = rate;
        render.output_count = count; render.probe_ratio = 1.f; render.clip_output = false;
        const auto controls = []
        {
            Parameters p; p.sourceRate = p.outputRate = rate; p.duration = 1.;
            p.sourceAt = [](double t) { return t / 1.25; };
            return p;
        };
        const auto waveformOracle = [&](const char* name, Parameters p,
                                        const std::function<double(std::size_t)>& phaseSamples)
        {
            Control control(std::move(p));
            const auto result = melodyne_re::detail::render_period_component_controlled(pcm, sourcePeriods, render, &control);
            // Exclude only the declared startup/end regions, never a region
            // selected in response to an observed failure or a source map seam.
            constexpr std::size_t begin = 2400, end = 45600;
            double maximum = 0., squared = 0., observedAtWorst = 0., expectedAtWorst = 0.;
            std::size_t worstIndex = begin, compared = 0;
            bool valid = result.ok && result.samples.size() == count;
            if (valid)
            {
                for (std::size_t n = begin; n < end; ++n)
                {
                    const double expected = amplitude * std::sin(omega * phaseSamples(n));
                    const double observed = double(result.samples[n]);
                    const double difference = std::abs(observed - expected);
                    if (!std::isfinite(difference)) { valid = false; maximum = std::numeric_limits<double>::infinity(); break; }
                    if (difference > maximum)
                    {
                        maximum = difference; worstIndex = n;
                        observedAtWorst = observed; expectedAtWorst = expected;
                    }
                    squared += difference * difference; ++compared;
                }
            }
            else maximum = std::numeric_limits<double>::infinity();
            const double rms = compared ? std::sqrt(squared / double(compared)) : std::numeric_limits<double>::infinity();
            log << name << ".raw_max_abs_error=" << juce::String(maximum, 12)
                << "\n" << name << ".raw_rms_error=" << juce::String(rms, 12)
                << "\n" << name << ".worst_sample=" << juce::String(double(worstIndex), 0)
                << "\n" << name << ".observed_at_worst=" << juce::String(observedAtWorst, 12)
                << "\n" << name << ".expected_at_worst=" << juce::String(expectedAtWorst, 12)
                << "\n" << name << ".compared_samples=" << juce::String(double(compared), 0)
                << "\n" << name << ".render_error=" << juce::String::fromUTF8(result.error.c_str()) << "\n";
            std::cout << name << ".raw_max_abs_error=" << maximum
                      << " raw_rms_error=" << rms << " worst_sample=" << worstIndex << std::endl;
            check(name, valid && compared == end - begin && maximum <= 1.e-4);
        };
        auto p = controls(); p.targetBoundaries = {0., .5, 1.};
        p.sourceAt = [](double t) { return t <= .5 ? .8 * t : .4 + 1.2 * (t - .5); };
        waveformOracle("independent_sine_piecewise_source_speed_keeps_250Hz_R1_Q1", p,
            [](std::size_t n) { return double(n); });
        p = controls(); p.pitchRatioAt = [](double) { return 1.25; };
        waveformOracle("independent_sine_T1_25_and_R1_25_has_312_5Hz", p,
            [](std::size_t n) { return 1.25 * double(n); });
        p = controls(); p.pitchRatioAt = [](double t) { return 1. + .3 * t; };
        waveformOracle("independent_sine_dynamic_R_uses_discrete_updated_rate_phase", p,
            [](std::size_t n)
            {
                const double i = double(n);
                return i + .3 * i * (i + 1.) / (2. * rate);
            });

        // This is an event-geometry test, NOT a spectral/subjective Q quality
        // oracle. Request full-component processed topology while actual Q
        // crosses unity. No probe value is modified to force the FFT decision.
        p = controls(); p.processedFrames = true;
        p.probeRatioAt = [](double t) { return float(.998 + .004 * t); };
        Control processedControl(p);
        const auto processed = melodyne_re::detail::render_period_component_controlled(pcm, sourcePeriods, render, &processedControl);
        std::size_t runtimeEvents = 0, fullAdds = 0, tailCopies = 0, unityBandEvents = 0;
        bool geometry = processed.ok && processed.samples.size() == count;
        bool beforeUnity = false, afterUnity = false;
        for (const auto& event : processed.events)
        {
            if (event.startup) continue;
            ++runtimeEvents;
            const bool wholeSupport = event.full_rebuild && event.left_length > 0 && event.right_length > 0
                && event.add_count == event.left_length + event.right_length && event.copy_count == 0;
            geometry &= wholeSupport;
            if (wholeSupport) ++fullAdds;
            if (event.copy_count != 0) ++tailCopies;
            const double time = double(event.output_index) / rate;
            if (time >= .25 && time <= .75)
            {
                ++unityBandEvents;
                beforeUnity |= time < .5; afterUnity |= time > .5;
            }
        }
        log << "processed_Q_sweep.runtime_events=" << juce::String(double(runtimeEvents), 0)
            << "\nprocessed_Q_sweep.full_ADD_L_plus_H_events=" << juce::String(double(fullAdds), 0)
            << "\nprocessed_Q_sweep.tail_COPY_events=" << juce::String(double(tailCopies), 0)
            << "\nprocessed_Q_sweep.unity_band_events=" << juce::String(double(unityBandEvents), 0)
            << "\nprocessed_Q_sweep.render_error=" << juce::String::fromUTF8(processed.error.c_str()) << "\n";
        check("processed_Q_crossing_unity_keeps_FULL_ADD_support_without_tail_COPY",
            geometry && runtimeEvents > 0 && unityBandEvents > 0 && beforeUnity && afterUnity && tailCopies == 0);
        log << "independent_oracle.scope=analytic periodic waveform only; absolute amplitude tolerance 0.0001; "
               "comparison samples [2400,45600); no fitted phase/gain/lag; no original-app equivalence claim\n";
    }
    log << "scope=control arithmetic, independent analytic sine and processed event geometry; no external media or original-app waveform claim\n";
    if (out.createDirectory().failed()) return false;
    return out.getChildFile("verification.txt").replaceWithText(log) && ok;
}
}
