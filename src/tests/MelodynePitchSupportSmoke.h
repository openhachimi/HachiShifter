#pragma once
#include "../backend/MelodyneImporter.h"
#include "../AudioEngine.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>

namespace hachi::tests
{
// Pure regression plus an optional, read-only fresh import of the diagnosed te
// MPD. No HJPX migration, source annotation publication, synthesis or media copy.
// Returns a process exit code (0 = PASS); report existence is not a test result.
inline int runMelodynePitchSupportSmoke(const juce::File& output,
                                       const juce::File& optionalMpd = {})
{
    using backend::melodynePitchSupportDiagnostic::SourcePitchPoint;
    using backend::melodynePitchSupportDiagnostic::pitchAt;
    if (output.createDirectory().failed()) return 1;
    bool ok = true;
    juce::Array<juce::var> checks, frames;
    const auto check = [&](const juce::String& name, bool pass)
    {
        ok &= pass;
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("pass", pass);
        checks.add(juce::var(row));
        std::cout << name << '=' << (pass ? "PASS" : "FAIL") << std::endl;
    };
    const auto near = [](double a, double b, double tolerance = 1.e-6)
    { return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= tolerance; };
    const double fs = 44100., supportStart = 4743. / 1024., supportEnd = 10601. / 1024.;
    const std::vector<SourcePitchPoint> points {
        {5, 6577.64208984375f, 6577.64208984375f, false},
        {6, 6715.42626953125f, 6715.42626953125f, false},
        {7, 6807.76806640625f, 6807.76806640625f, false},
        {8, 6842.162109375f, 6842.162109375f, false},
        {9, 6835.787109375f, 6835.787109375f, false}
    };
    const auto at = [&](double slice) { return pitchAt(points, slice, supportStart, supportEnd); };
    const auto head = at(supportStart), tail = at(supportEnd);
    check("actual_item_begin_holds_first_property", head && head->first == points.front().raw);
    check("actual_10601_sample_end_holds_slice9", tail && tail->first == points.back().raw);
    check("outside_actual_item_is_UV_even_near_property_grid",
          !at(supportStart - 1.e-7) && !at(supportEnd + 1.e-7));
    const auto interior = at(7.25);
    check("interior_pitch_interpolation_retained", interior && interior->first >= points[2].raw
          && interior->first <= points[3].raw);

    // The original tail Bezier (points 15358 -> 15359), not a linear time-map
    // substitute: target 4.0 seconds corresponds to source about .224589 s.
    const auto cubic = [](double a, double b, double c, double d, double t)
    { const auto u = 1. - t; return u*u*u*a + 3*u*u*t*b + 3*u*t*t*c + t*t*t*d; };
    double lo = 0., hi = 1.;
    for (int i = 0; i < 52; ++i)
    {
        const auto t = (lo + hi) * .5;
        const auto x = cubic(.03577763650287576, .047703503411288845,
                             .11064115935920906, .13276936467717793, t);
        if (x < 4. - 3.8887093082901574) lo = t; else hi = t;
    }
    const auto sourceAt4 = 4743. / fs + cubic(.0664172335600907, .08855628927437641,
        .11069541140589569, .1328344671201814, (lo + hi) * .5);
    const auto fourSeconds = at(sourceAt4 * fs / 1024.);
    check("real_Bezier_4s_source_is_224589ms", near(sourceAt4, .224589, 2.e-6));
    check("real_Bezier_tail_beyond_old_half_grid_remains_voiced",
          sourceAt4 * fs / 1024. > 9.500001 && fourSeconds
          && fourSeconds->first == points.back().raw);

    auto silentTail = points; silentTail.back().silent = true;
    check("explicit_silent_last_property_keeps_held_tail_UV",
          !pitchAt(silentTail, 9., supportStart, supportEnd)
          && !pitchAt(silentTail, 10., supportStart, supportEnd));
    auto silentHead = points; silentHead.front().silent = true;
    check("explicit_silent_first_property_keeps_held_head_UV",
          !pitchAt(silentHead, supportStart, supportStart, supportEnd));
    check("silent_property_neighbour_keeps_interpolation_UV",
          !pitchAt(silentTail, 8.5, supportStart, supportEnd));
    check("empty_points_never_invent_pitch", !pitchAt({}, 7., supportStart, supportEnd));
    const std::vector<SourcePitchPoint> noPitch {{5, 0, 0, false}, {9, 0, 0, false}};
    check("no_detected_pitch_stays_UV", !pitchAt(noPitch, 10., supportStart, supportEnd));
    auto invalid = points; invalid.back().raw = std::numeric_limits<float>::quiet_NaN();
    check("nonfinite_pitch_stays_UV", !pitchAt(invalid, 10., supportStart, supportEnd));
    check("missing_or_invalid_actual_support_stays_UV",
          !pitchAt(points, 7., 0., 0.) && !pitchAt(points, 7., supportEnd, supportStart));

    juce::String importError;
    if (optionalMpd != juce::File{})
    {
        check("optional_real_MPD_exists", optionalMpd.existsAsFile());
        if (optionalMpd.existsAsFile())
        {
            const auto mpdSha = juce::SHA256(optionalMpd).toHexString();
            auto imported = backend::MelodyneImporter::importProject(optionalMpd, importError);
            check("fresh_read_only_MPD_import", imported.has_value());
            if (imported)
            {
                std::map<juce::String, std::pair<juce::File, juce::String>> media;
                for (const auto& track : imported->project.tracks)
                    for (const auto& clip : track.clips)
                        if (clip.sourceFile.existsAsFile())
                        {
                            const auto key = clip.sourceFile.getFullPathName();
                            if (media.find(key) == media.end())
                                media.emplace(key, std::make_pair(clip.sourceFile,
                                    juce::SHA256(clip.sourceFile).toHexString()));
                        }
                const auto window = [&](int item, double itemBeginSample, double expectedStart,
                                        double from, double to, double sourceCenter)
                {
                    int matched = 0, contourCount = 0, requestCount = 0;
                    bool voiced = true, requestVoiced = true, target = true, controls = true;
                    for (const auto& track : imported->project.tracks)
                        for (const auto& clip : track.clips)
                        {
                            if (!near(clip.sourceOffsetSeconds, itemBeginSample / fs, 1.e-6)
                                || !near(clip.startSeconds, expectedStart, .02)) continue;
                            ++matched;
                            const auto request = AudioEngine::diagnosticNativeRequest(clip, track);
                            for (const auto& note : clip.notes)
                            {
                                controls &= near(note.midiNote, 79.) && near(note.sourceMidiCenter, double(float(sourceCenter)))
                                    && near(note.drift, 1.) && near(note.modulation, 1.);
                                for (const auto& p : note.contour)
                                {
                                    const auto absolute = clip.startSeconds + note.startSeconds + p.timeSeconds;
                                    if (absolute < from || absolute > to) continue;
                                    ++contourCount; voiced &= p.voiced;
                                }
                            }
                            for (std::size_t i = 0; i < request.sourceMidi.size(); ++i)
                            {
                                const auto absolute = clip.startSeconds + double(i) * request.framePeriodMs / 1000.;
                                if (absolute < from || absolute > to) continue;
                                ++requestCount;
                                const auto src = request.sourceMidi[i];
                                const auto dst = i < request.targetMidi.size() ? request.targetMidi[i] : 0.f;
                                requestVoiced &= src > 0 && dst > 0;
                                target &= near(dst, 79., .2);
                                auto* frame = new juce::DynamicObject();
                                frame->setProperty("item", item); frame->setProperty("target_seconds", absolute);
                                frame->setProperty("source_midi", src); frame->setProperty("target_midi", dst);
                                frames.add(juce::var(frame));
                            }
                        }
                    const auto prefix = "item" + juce::String(item) + "_";
                    check(prefix + "physical_te_source_found_once", matched == 1);
                    check(prefix + "tail_contour_voiced", contourCount >= 3 && voiced);
                    check(prefix + "tail_request_source_and_target_voiced", requestCount >= 3 && requestVoiced);
                    check(prefix + "author_target_stays_near_79", requestCount >= 3 && target);
                    check(prefix + "author_centers_drift_modulation_unchanged", matched == 1 && controls);
                };
                window(3985, 4743., 3.8887093082901574, 3.993247683, 4.021478673, 68.29);
                window(3984, 10601., 4.021478672967335, 4.1272, 4.1522, 68.17);
                bool sameMedia = true;
                for (const auto& [path, entry] : media)
                {
                    juce::ignoreUnused(path);
                    sameMedia &= juce::SHA256(entry.first).toHexString() == entry.second;
                }
                check("original_recordings_bytes_unchanged", sameMedia);
            }
            check("original_MPD_bytes_unchanged", juce::SHA256(optionalMpd).toHexString() == mpdSha);
        }
    }
    auto* report = new juce::DynamicObject();
    report->setProperty("pass", ok); report->setProperty("checks", juce::var(checks));
    report->setProperty("fresh_import_frames", juce::var(frames));
    report->setProperty("import_error", importError);
    report->setProperty("scope", "Pure source-item support and optional fresh MPD contour/request; no render or legacy HJPX migration");
    const auto written = output.getChildFile("verification.json").replaceWithText(
        juce::JSON::toString(juce::var(report), true));
    return ok && written ? 0 : 1;
}
}
