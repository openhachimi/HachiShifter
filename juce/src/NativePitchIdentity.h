#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>
namespace hachi
{
inline bool nativePitchIsUnedited(const NoteData& note)
{
    if (!note.sourcePitchMeasured || note.sourceMidiCenter < 0.0f || note.vibratoEnabled
        || std::abs(note.midiNote - note.sourceMidiCenter) > 1.0e-4f) return false;
    return std::all_of(note.contour.begin(), note.contour.end(), [&](const auto& point)
    { return !point.voiced || std::abs(renderedPitchCents(note, point) - point.relativeCents) < .01f; });
}
inline bool nativeSourcePitchIsKnown(const NoteData& note)
{
    return (note.sourcePitchMeasured && note.sourceMidiCenter >= 0.0f) || (!note.contour.empty()
        && std::none_of(note.contour.begin(), note.contour.end(), [](const auto& p){return p.voiced;}));
}
}
