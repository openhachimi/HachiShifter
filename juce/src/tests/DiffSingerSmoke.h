#pragma once
#include "../AudioEngine.h"
#include "../backend/DiffSingerRenderer.h"
#include <iostream>

namespace hachi
{
inline bool runDiffSingerSmoke(const juce::File& bank, const juce::File& folder)
{
    folder.createDirectory();
    ProjectModel model;
    ProjectData data;
    TrackData track;
    track.id = "ds-track"; track.name = "Umidaji (Team BRAPA)";
    track.pitchAlgorithm = PitchAlgorithm::utau;
    track.voicebankDirectory = bank;
    ClipData clip;
    clip.id = "ds-clip"; clip.durationSeconds = 2.4;
    for (int i = 0; i < 2; ++i)
    {
        NoteData note;
        note.id = "ds-note-" + juce::String(i); note.label = i == 0 ? "ni" : "hao";
        note.startSeconds = i == 0 ? .3 : 1.; note.durationSeconds = i == 0 ? .7 : .9;
        note.midiNote = i == 0 ? 60.0f : 64.0f;
        clip.notes.push_back(note);
    }
    track.clips.push_back(clip); data.tracks.push_back(track); model.replace(data);
    model.setNotesUtauFlagCurveEnabled({"ds-note-0","ds-note-1"},true);
    model.setNoteUtauFlagCurve("ds-note-0","DS:DYN",{{0,-60},{.7,0}});
    model.setNoteUtauFlagCurve("ds-note-0","DS:GENC",{{0,-40},{.7,40}});
    backend::UtauRenderRequest request;
    request.voicebankDirectory = bank; request.targetDurationSeconds = clip.durationSeconds;
    request.notes = AudioEngine::diagnosticUtauRequestNotes(model.snapshot(), clip.id);
    int cancellationPolls = 0;
    const auto interrupted = backend::DiffSingerRenderer::invoke(
        backend::DiffSingerRenderer::requestJson(request, "inspect"), [&] { return ++cancellationPolls >= 2; });
    if ((bool) interrupted["ok"] || !interrupted["error"].toString().contains("cancelled")) return false;
    std::cout << "worker_cancelled_after_start=1" << std::endl;
    const auto result = backend::DiffSingerRenderer::invoke(backend::DiffSingerRenderer::requestJson(request, "pitch"));
    folder.getChildFile("pitch.json").replaceWithText(juce::JSON::toString(result));
    if (!(bool) result["ok"])
    { std::cout << result["error"].toString() << std::endl; return false; }
    std::vector<std::pair<juce::String, std::vector<PitchCurveEditPoint>>> curves;
    if (const auto* values = result["curves"].getArray())
        for (const auto& value : *values)
        {
            const auto i = value["id"].toString().getIntValue();
            if (i < 0 || i > 1) return false;
            std::vector<PitchCurveEditPoint> points;
            if (const auto* p = value["points"].getArray())
                for (const auto& xy : *p) points.push_back({(double) xy[0], (float) xy[1]});
            curves.emplace_back(clip.notes[(std::size_t)i].id, std::move(points));
        }
    bool ok = curves.size() == 2 && curves.front().second.size() > 100;
    const auto before = model.revisionNumber();
    ok = model.applyDiffSingerPitch(before, curves) && ok;
    ok = !model.applyDiffSingerPitch(before, curves) && ok; // stale result
    model.undo();
    const auto undone = model.snapshot();
    for (const auto& n : undone.tracks.front().clips.front().notes)
        ok = n.pitchControlPoints.empty() && ok;
    model.redo();
    const auto redone = model.snapshot();
    for (const auto& n : redone.tracks.front().clips.front().notes)
        ok = n.pitchControlPoints.size() > 100 && ok;
    model.setDiffSingerOptions(track.id, "zh", "embeds/umidaji-rainbow");
    juce::String error;
    ok = model.save(folder.getChildFile("DiffSinger-你好.hjpx"), error) && ok;
    ProjectModel loaded;
    ok = loaded.load(folder.getChildFile("DiffSinger-你好.hjpx"), error) && ok;
    ok = loaded.snapshot().tracks.front().diffSingerSpeaker == "embeds/umidaji-rainbow" && ok;
    AudioEngine engine;
    engine.prepareToPlay(512, 44100);
    engine.selectEveryUtauNote(loaded.snapshot());
    engine.syncProject(loaded.snapshot());
    for (int i = 0; i < 1200 && !engine.hasCurrentRenderedAudio(); ++i)
    {
        if (i > 30 && !engine.renderProgress().has_value()) break;
        juce::Thread::sleep(50);
    }
    std::cout << "backend=" << engine.activeRenderBackends() << "|warning=" << engine.activeRenderWarnings() << std::endl;
    ok = engine.hasCurrentRenderedAudio() && engine.activeRenderBackends().contains("DiffSinger") && ok;
    juce::AudioBuffer<float> buffer(2, 512);
    float peak = 0;
    engine.setPosition(0); engine.play();
    for (int i = 0; i < 210; ++i)
    {
        buffer.clear(); juce::AudioSourceChannelInfo info(&buffer, 0, 512);
        engine.getNextAudioBlock(info);
        peak = std::max(peak, buffer.getMagnitude(0, 512));
    }
    engine.stop();
    ok = peak > .001f && ok;
    ok = engine.exportWav(folder.getChildFile("Umidaji-你好-generated-pitch.wav"), error) && ok;
    // A curve edit must invalidate the already rendered phrase cache.
    loaded.setNoteUtauFlagCurve("ds-note-0","DS:DYN",{{0,-240}});
    loaded.setNoteUtauFlagCurve("ds-note-1","DS:DYN",{{0,-240}});
    engine.syncProject(loaded.snapshot());
    for (int i = 0; i < 1200 && !engine.hasCurrentRenderedAudio(); ++i)
    {
        if (i > 30 && !engine.renderProgress().has_value()) break;
        juce::Thread::sleep(50);
    }
    float quieterPeak=0; engine.setPosition(0); engine.play();
    for (int i=0;i<210;++i)
    {
        buffer.clear(); juce::AudioSourceChannelInfo info(&buffer,0,512);
        engine.getNextAudioBlock(info); quieterPeak=std::max(quieterPeak,buffer.getMagnitude(0,512));
    }
    engine.stop();
    const auto cacheOk=engine.hasCurrentRenderedAudio() && quieterPeak>1e-5f && quieterPeak<peak*.25f;
    ok=cacheOk && ok;
    std::cout << "expression_cache_invalidated=" << cacheOk << "|quiet_peak=" << quieterPeak << std::endl;
    ok=engine.exportWav(folder.getChildFile("Umidaji-expression-DYN-minus24dB.wav"),error) && ok;
    for (auto& note : request.notes) note.gain = 0;
    auto muted = backend::UtauRenderer::render(request);
    ok = muted.warning.isEmpty() && muted.buffer.getNumSamples() > 0
        && muted.buffer.getMagnitude(0, muted.buffer.getNumSamples()) == 0 && ok;
    auto cancelled = backend::DiffSingerRenderer::invoke(
        backend::DiffSingerRenderer::requestJson(request, "render"), [] { return true; });
    ok = !(bool) cancelled["ok"] && cancelled["error"].toString().contains("cancelled") && ok;
    std::cout << "diffsinger_smoke=" << ok << "|peak=" << peak << "|error=" << error << std::endl;
    return ok;
}
}
