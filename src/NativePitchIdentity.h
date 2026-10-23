#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>
namespace hachi
{
inline bool nativePitchIsUnedited(const NoteData& note)
{
    if (note.nativeUnpitched) return true;
    if (!note.sourcePitchMeasured || note.sourceMidiCenter < 0.0f || note.vibratoEnabled
        || std::abs(note.midiNote - note.sourceMidiCenter) > 1.0e-4f) return false;
    return std::all_of(note.contour.begin(), note.contour.end(), [&](const auto& point)
    { return !point.voiced || std::abs(renderedPitchCents(note, point) - point.relativeCents) < .01f; });
}
inline bool nativeSourcePitchIsKnown(const NoteData& note)
{
    if (note.nativeUnpitched) return true;
    return (note.sourcePitchMeasured && note.sourceMidiCenter >= 0.0f) || (!note.contour.empty()
        && std::none_of(note.contour.begin(), note.contour.end(), [](const auto& p){return p.voiced;}));
}
// Imported timing-only HJM regions temporarily carry a two-point C4 placeholder
// until GAME/FCPE returns. It is display metadata, not a request to resynthesise
// the recording. Be conservative: authored pitch/expressive changes still wait
// for measured F0 and may not take this shortcut.
inline bool nativePendingPitchIsNeutral(const NoteData& note)
{
    return !note.sourcePitchMeasured && note.sourceMidiCenter < 0.f
        && note.nativeSourceStartSeconds >= 0.0
        && note.nativeSourceEndSeconds > note.nativeSourceStartSeconds
        && std::abs(note.midiNote - 60.f) < 1.e-4f
        && std::abs(note.drift - 1.f) < 1.e-4f
        && std::abs(note.modulation - 1.f) < 1.e-4f
        && !note.vibratoEnabled && note.pitchControlPoints.empty()
        && note.contour.size() == 2
        && std::all_of(note.contour.begin(), note.contour.end(), [](const auto& point)
        { return !point.hasManualTarget && point.voiced
            && point.relativeCents == 0.f && point.withoutVibratoCents == 0.f; });
}
}
