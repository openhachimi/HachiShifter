#pragma once
#include "../PianoRollComponent.h"
#include "../AudioEngine.h"
#include "../backend/NsfHifiganRenderer.h"
#include <iostream>

namespace hachi
{
inline bool runNsfRegionsSmoke(const juce::File& folder, const juce::File& modelFolder)
{
    using namespace backend;
    folder.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool pass)
    {
        auto* row = new juce::DynamicObject();
        row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(row); ok &= pass; std::cout << name << '=' << pass << std::endl;
    };
    const auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-6; };
    const auto bank = folder.getChildFile("bank"); bank.createDirectory();
    const auto sub = bank.getChildFile("C5"); sub.createDirectory();
    const auto wavFile = sub.getChildFile("tone.wav");
    {
        juce::AudioBuffer<float> tone(1, 52920);
        for (int i = 0; i < tone.getNumSamples(); ++i)
        {
            const auto t = i / 44100.0;
            const auto amplitude = t < .2 ? .05 : t < .3 ? .17 : t < .8 ? .10 : .025;
            tone.setSample(0, i, static_cast<float>(amplitude * (1 + .2 * std::sin(51 * t))
                * std::sin(juce::MathConstants<double>::twoPi * 220 * t)));
        }
        juce::WavAudioFormat format; auto stream = wavFile.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), 44100, 1, 16, {}, 0));
        if (!writer) return false;
        writer->writeFromAudioSampleBuffer(tone, 0, tone.getNumSamples());
    }
    wavFile.copyFileTo(sub.getChildFile("two.wav")); wavFile.copyFileTo(sub.getChildFile("three.wav"));
    sub.getChildFile("oto.ini").replaceWithText("tone.wav=a,0,100,-1000,100,30\n"
        "two.wav=two,100,100,-900,100,30\nthree.wav=three,100,100,-900,100,30\n");
    sub.getChildFile("oto.jie.ini").replaceWithText("tone.wav=a,100,100,-900,100,30\n"
        "two.wav=two,100,100,-900,100,30\nthree.wav=three,100,100,-900,100,30\n");
    sub.getChildFile("oto4.ini").replaceWithText("tone.wav=100,100,200,700\n");
    sub.getChildFile("otomou.ini").replaceWithText("tone.wav=CVVC,100,100,200,700\n"
        "two.wav=VC,100,100,200,700\nthree.wav=CVS,100,100,700,800\n");
    const auto originalHash = juce::SHA256(sub.getChildFile("oto.ini")).toHexString();
    const auto resolve = [&](bool regions, bool mou, int velocity = 100, double stp = 0,
                             const UtauOtoOverride* own = nullptr)
    { return UtauRenderer::resolveVoiceSample(bank, "a", 60, velocity, regions, mou, stp,
                                              false, 0, false, 0, own); };
    const auto timingFor = [](const UtauRenderer::ResolvedSample& r)
    {
        NsfUtauSampleTiming t; t.offsetSeconds = r.offsetSeconds; t.endSeconds = r.endSeconds;
        t.fileSeconds = r.fileSeconds; t.preutteranceSeconds = r.preutteranceSeconds;
        t.consonantSeconds = r.consonantSeconds; t.overlapSeconds = r.overlapSeconds;
        t.hasRegions = r.hasRegions; t.regionSeconds = r.regionSeconds; t.mouClasses = r.mouClasses;
        return t;
    };
    const auto plain = resolve(false, false), jie = resolve(true, false), mou = resolve(true, true);
    check("recursive_oto_and_independent_jie_timing", plain.found && jie.found && near(plain.offsetSeconds, 0)
        && near(jie.offsetSeconds, .1) && near(jie.endSeconds, 1.0));
    check("resolver_preserves_jie_regions_and_mou_classes", jie.hasRegions && jie.mouClasses.isEmpty()
        && mou.hasRegions && mou.mouClasses == "CVVC" && near(mou.regionSeconds[3], .2));
    const auto realTwo = UtauRenderer::resolveVoiceSample(bank, "two", 60, 200, true, true);
    const auto realThree = UtauRenderer::resolveVoiceSample(bank, "three", 60, 100, true, true);
    check("mou_oto_loads_two_and_three_region_entries", realTwo.hasRegions && realTwo.mouClasses == "VC"
        && realThree.hasRegions && realThree.mouClasses == "CVS"
        && near(realTwo.regionSeconds[1], .8) && near(realThree.regionSeconds[2], .2));
    check("mou_initial_vowel_is_not_scaled_by_consonant_velocity", near(realTwo.preutteranceSeconds, .1));
    const auto jt = timingFor(jie), mt = timingFor(mou);
    const auto jp = buildNsfUtauNotePlan(jt, .3, .7, 1, 0);
    const auto mp = buildNsfUtauNotePlan(mt, .3, .7, 1, 0);
    check("jie_four_source_boundaries_reach_mel_mapper", jp.valid && jp.usesRegions && jp.regionCount == 4
        && jp.timeMap.size() == 5 && near(jp.timeMap[2].sourceSeconds, .3)
        && near(jp.timeMap[3].sourceSeconds, .8) && near(jp.timeMap.back().sourceSeconds, 1));
    check("onset_boundary_lands_on_note_beat", near(jp.timeMap[1].targetSeconds, .1)
        && near(jp.timeMap[1].targetSeconds + jp.soundStartOffsetSeconds, 0));
    check("mou_coda_keeps_natural_duration", near(mp.outputRegionSeconds[3], .2)
        && !near(jp.outputRegionSeconds[3], .2));
    const std::array<double, 3> manual { .125, .4, .8 };
    const auto hand = buildNsfUtauNotePlan(mt, .3, .7, 1, 0, &manual);
    check("manual_boundaries_control_independent_time_warps", hand.valid
        && near(hand.timeMap[2].targetSeconds, .32) && near(hand.timeMap[3].targetSeconds, .64)
        && near(hand.timeMap[2].sourceSeconds, mp.timeMap[2].sourceSeconds));
    const auto fast = buildNsfUtauNotePlan(timingFor(resolve(true, true, 200)), .3, .7, .5, 0);
    check("mou_velocity_changes_consonants_including_coda", fast.valid && near(fast.outputRegionSeconds[3], .1)
        && near(fast.outputRegionSeconds[0], .05));
    auto two = mt; two.mouClasses = "VC"; two.regionSeconds = { .1, .8, 0, 0 };
    auto three = mt; three.mouClasses = "CVS"; three.regionSeconds = { .1, .6, .2, 0 };
    const auto p2 = buildNsfUtauNotePlan(two, .3, .7, 1, 0, &manual);
    const auto p3 = buildNsfUtauNotePlan(three, .3, .7, 1, 0, &manual);
    check("two_and_three_regions_have_no_phantom_fourth", p2.valid && p3.valid
        && p2.regionCount == 2 && p2.timeMap.size() == 3
        && p3.regionCount == 3 && p3.timeMap.size() == 4 && near(p3.outputRegionSeconds[3], 0));
    const auto spelling = buildNsfUtauNotePlan(jt, .3, 0, 1, 0, &manual, true);
    check("jie_spelling_reads_only_first_two_source_regions", spelling.valid && spelling.regionCount == 2
        && near(spelling.sourceEndSeconds, .3) && near(spelling.outputRegionSeconds[2], 0));
    const auto clipped = buildNsfUtauNotePlan(mt, 0, .2, 1, 0);
    bool monotone = clipped.valid;
    for (std::size_t i = 1; i < clipped.timeMap.size(); ++i)
        monotone &= clipped.timeMap[i].targetSeconds >= clipped.timeMap[i - 1].targetSeconds
            && clipped.timeMap[i].sourceSeconds >= clipped.timeMap[i - 1].sourceSeconds;
    check("short_note_and_zero_leadin_keep_monotone_map", monotone && near(clipped.outputSeconds, .2)
        && near(clipped.timeMap.front().targetSeconds, 0) && near(clipped.timeMap.back().targetSeconds, .2));
    const auto overlap = buildNsfUtauNotePlan(mt, .3, .7, 1, 0, nullptr, false, .65);
    check("audible_overlap_span_is_used_for_region_allocation", near(overlap.outputSeconds, .65)
        && near(overlap.outputRegionSeconds[3], .2));
    auto absent = timingFor(plain); absent.hasRegions = false;
    const auto fallback = buildNsfUtauNotePlan(absent, .3, .7, 1, 0);
    check("ordinary_oto_retains_classic_mapping", fallback.valid && !fallback.usesRegions && fallback.timeMap.size() == 3);
    const auto shifted = resolve(true, true, 100, .1);
    check("stp_moves_all_source_regions_together", near(shifted.offsetSeconds, .2)
        && near(shifted.endSeconds, 1.1) && shifted.regionSeconds == mou.regionSeconds);
    UtauOtoOverride own; own.enabled = own.hasRegions = true; own.offsetMs = 200;
    own.cutoffMs = -800; own.consonantMs = own.preutteranceMs = 80;
    own.onsetMs = 80; own.glideMs = 250; own.nucleusMs = 600; own.classes = "SVVC";
    const auto local = resolve(true, true, 100, 0, &own);
    check("own_oto_preserves_regions_and_classes_without_bank_write", local.hasRegions && local.mouClasses == "SVVC"
        && near(local.regionSeconds[1], .17) && originalHash == juce::SHA256(sub.getChildFile("oto.ini")).toHexString());

    NoteData n; n.id = "note"; n.label = "a"; n.startSeconds = .3; n.durationSeconds = .7;
    ClipData clip; clip.id = "clip"; clip.durationSeconds = 1.2; clip.notes = { n };
    TrackData track; track.id = "track"; track.voicebankDirectory = bank; track.utauMode = UtauMode::mou;
    track.pitchAlgorithm = PitchAlgorithm::utau; track.outputEngine = UtauOutputEngine::pcNsfHifigan; track.clips = { clip };
    ProjectData data; data.tracks = { track };
    const auto mouKey = AudioEngine::diagnosticUtauRenderKey(data, "clip");
    auto jieData = data; jieData.tracks[0].utauMode = UtauMode::jie;
    check("jie_and_mou_render_cache_keys_are_distinct", mouKey != AudioEngine::diagnosticUtauRenderKey(jieData, "clip"));
    auto manualData = data; manualData.tracks[0].clips[0].notes[0].utauJieSplitSet = true;
    manualData.tracks[0].clips[0].notes[0].utauJieSplit2 = .4;
    check("manual_region_edits_invalidate_native_cache", mouKey != AudioEngine::diagnosticUtauRenderKey(manualData, "clip"));
    const auto mouFile = sub.getChildFile("otomou.ini"); const auto originalMou = mouFile.loadFileAsString();
    const auto originalMouTime = mouFile.getLastModificationTime();
    mouFile.replaceWithText(originalMou + "; changed cache fixture\n");
    mouFile.setLastModificationTime(originalMouTime + juce::RelativeTime::seconds(2));
    check("nested_oto_change_invalidates_native_cache", mouKey != AudioEngine::diagnosticUtauRenderKey(data, "clip"));
    mouFile.replaceWithText(originalMou); UtauRenderer::invalidateVoicebankCache();
    TrackData unbound; unbound.pitchAlgorithm = PitchAlgorithm::nsfHifigan; unbound.utauMode = UtauMode::jie;
    check("native_region_mode_exposes_voicebank_selection_before_binding", trackUsesVoicebankSynthesis(unbound));
    ProjectModel model; model.resetDocument(data);
    I18n strings; PianoRollComponent roll(model, strings); roll.setReadsVoicebankInBackground(false);
    roll.setSize(1200, 650); roll.setFocusedTrack("track"); roll.setFocusedClip("clip"); roll.diagnosticRefresh();
    const auto edges = roll.diagnosticRegionEdges(0);
    check("native_nsf_roll_displays_actual_four_region_edges", roll.diagnosticHasSoundingSpan("note")
        && roll.diagnosticRegionCount(0) == 4 && near(edges[0], .3)
        && near(edges[2], .2 + mp.timeMap[3].targetSeconds));
    roll.setTool(PianoRollComponent::Tool::note);
    check("native_nsf_region_boundary_is_draggable", roll.diagnosticJieHandleAt(
        { roll.diagnosticEdgeX(edges[1]), roll.diagnosticNoteY(0) }));
    model.setNotesUtauJieSplit({ "note" }, manual[0], manual[1], manual[2]);
    model.dispatchPendingMessages(); roll.diagnosticRefresh();
    check("manual_region_edit_updates_native_roll", near(roll.diagnosticRegionEdges(0)[1], .2 + .32));
    check("native_oto_timing_does_not_produce_false_warning", AudioEngine::renderCapabilityWarnings(model.snapshot()).isEmpty());
    juce::String error; const auto saved = folder.getChildFile("native-regions.hjpx"); ProjectModel reopened;
    check("native_mode_and_manual_split_roundtrip", model.save(saved, error) && reopened.load(saved, error)
        && reopened.snapshot().tracks[0].utauMode == UtauMode::mou
        && reopened.snapshot().tracks[0].clips[0].notes[0].utauJieSplitSet);
    {
        juce::PNGImageFormat png; auto stream = folder.getChildFile("native-regions.png").createOutputStream();
        check("native_region_ui_snapshot", stream && png.writeImageToStream(roll.createComponentSnapshot(roll.diagnosticHitBounds(0).getSmallestIntegerContainer().expanded(45, 35), true, 3.0f), *stream));
    }
    if (NsfHifiganRenderer::modelAvailable(modelFolder))
    {
        UtauRenderRequest request; request.voicebankDirectory = bank; request.fourRegion = request.consonantClasses = true;
        request.targetDurationSeconds = 1.2; UtauNoteRenderSpec note; note.alias = "a";
        note.startSeconds = .3; note.durationSeconds = .7; note.midiNote = 60; request.notes = { note };
        std::vector<UtauPhonemeSpan> spans;
        request.notePhonemes = [&](std::size_t index, const auto& value) { if (index == 0) spans = value; };
        OrtExecutionConfig execution;
        const auto first = renderNsfUtauPhrase(request, modelFolder, execution);
        check("real_onnx_mou_render_uses_four_regions", first.warning.isEmpty() && first.buffer.getMagnitude(0, first.buffer.getNumSamples()) > .001
            && spans.size() == 4 && near(spans[3].endSeconds - spans[3].startSeconds, .2));
        request.notes[0].jieSplitSet = true; request.notes[0].jieSplit = manual;
        const auto second = renderNsfUtauPhrase(request, modelFolder, execution);
        double difference = 0;
        if (first.buffer.getNumSamples() == second.buffer.getNumSamples())
            for (int i = 0; i < first.buffer.getNumSamples(); ++i)
                difference += std::abs(first.buffer.getSample(0, i) - second.buffer.getSample(0, i));
        check("manual_split_changes_actual_neural_audio", second.warning.isEmpty() && difference > 1
            && spans.size() == 4 && near(spans[1].endSeconds, -.1 + .32));
        request.notes[0].jieSplitSet = false;
        request.consonantClasses = false;
        const auto realJie = renderNsfUtauPhrase(request, modelFolder, execution);
        check("real_onnx_jie_render_uses_oto4", realJie.warning.isEmpty() && spans.size() == 4
            && near(spans[3].endSeconds - spans[3].startSeconds, jp.outputRegionSeconds[3]));
        request.notes[0].durationSeconds = 0;
        const auto realSpelling = renderNsfUtauPhrase(request, modelFolder, execution);
        check("real_onnx_jie_spelling_has_only_two_regions", realSpelling.warning.isEmpty() && spans.size() == 2);
        request.consonantClasses = true; request.notes[0].durationSeconds = .7;
        for (const auto& alias : { "two", "three" })
        {
            request.notes[0].alias = alias;
            const auto rendered = renderNsfUtauPhrase(request, modelFolder, execution);
            check(juce::String(alias) == "two" ? "real_onnx_mou_two_regions" : "real_onnx_mou_three_regions",
                rendered.warning.isEmpty() && spans.size() == (juce::String(alias) == "two" ? 2 : 3));
        }
        request.notes[0].alias = "a";
        auto next = request.notes[0]; next.startSeconds = 1; request.notes.push_back(next);
        const auto phrase = renderNsfUtauPhrase(request, modelFolder, execution);
        check("real_phrase_handover_reallocates_regions_before_mix", phrase.warning.isEmpty()
            && spans.size() == 4 && near(spans.back().endSeconds, .63));
        request.notes.resize(1);
        request.notes[0].tailFadeMode = 1;
        request.notes[0].tailFadeSettings.head.mode = 1;
        float headGain = 1, tailGain = 1;
        request.notePiece = [&](std::size_t, const auto&, double, double pre, const auto& gainAt, const auto&) {
            headGain = gainAt(-pre); tailGain = gainAt(.7);
        };
        const auto enveloped = renderNsfUtauPhrase(request, modelFolder, execution);
        check("native_model_applies_first_region_head_and_last_region_tail_envelopes", enveloped.warning.isEmpty()
            && headGain < .0001 && tailGain < .0001 && enveloped.buffer.getMagnitude(0, enveloped.buffer.getNumSamples()) > .001);
        juce::WavAudioFormat format; auto stream = folder.getChildFile("native-mou-manual.wav").createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(), second.sampleRate, 2, 16, {}, 0));
        check("real_neural_wav_export", writer && writer->writeFromAudioSampleBuffer(second.buffer, 0, second.buffer.getNumSamples()));
    }
    else check("real_onnx_model_available", false);
    auto* report = new juce::DynamicObject(); report->setProperty("ok", ok); report->setProperty("checks", checks);
    folder.getChildFile("nsf-regions-validation.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    return ok;
}
}
