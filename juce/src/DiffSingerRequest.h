#pragma once
#include "AudioEngine.h"
#include "backend/DiffSingerRenderer.h"

namespace hachi
{
inline backend::UtauRenderRequest diffSingerRequest(const ProjectData& data, const TrackData& track,
                                            juce::StringArray& ids, bool effectivePitch = false)
{
    backend::UtauRenderRequest request;
    request.voicebankDirectory = track.voicebankDirectory;
    request.diffSingerLanguage = track.diffSingerLanguage;
    request.diffSingerSpeaker = track.diffSingerSpeaker;
    request.diffSingerDictionary = track.diffSingerDictionary;
    for (const auto& clip : track.clips) if (!clip.muted)
    {
        const auto rendered = effectivePitch ? AudioEngine::utauRequestNotesForClip(data, clip.id)
                                             : std::vector<backend::UtauNoteRenderSpec>{};
        for (size_t i = 0; i < clip.notes.size(); ++i)
        {
            const auto& note = clip.notes[i];
            if (note.nativeUnpitched) continue;
            backend::UtauNoteRenderSpec spec;
            if (i < rendered.size()) spec = rendered[i];
            spec.alias = note.label; spec.midiNote = note.midiNote;
            spec.startSeconds = clip.startSeconds + note.startSeconds;
            spec.durationSeconds = note.durationSeconds;
            spec.diffSingerTiming = note.diffSingerTiming;
            spec.diffSingerPronunciation = note.diffSingerPronunciation;
            spec.diffSingerContext = clip.id;
            spec.flagCurve = note.utauFlagCurveEnabled;
            spec.flagCurves = sampleDiffSingerFlagCurves(note);
            request.targetDurationSeconds = std::max(request.targetDurationSeconds, spec.startSeconds+spec.durationSeconds+.2);
            request.notes.push_back(std::move(spec)); ids.add(note.id);
        }
    }
    return request;
}
}
