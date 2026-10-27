#pragma once
#include "ProjectModel.h"

namespace hachi
{
struct NativeSourceEdgeMember
{
    juce::String noteId, parentClipId, ownerId, expandedClipId, audioId, regionId;
    juce::File sourceFile;
    std::size_t component = 0;
    bool exposedEntry = false, exposedExit = false; // topological component ends
    // One activated outer window may cross several retained slices. These flags
    // describe its contribution here, not a new independent fade per note.
    bool entryWindowActive = false, exitWindowActive = false;
    double supportFromTargetSeconds = 0, supportToTargetSeconds = 0;
    // Absolute project target seconds, derived from the component source clock.
    double entryFromTargetSeconds = 0, entryToTargetSeconds = 0;
    double exitFromTargetSeconds = 0, exitToTargetSeconds = 0, terminationTargetSeconds = 0;
};
struct NativeSourceEdgeComponent
{
    std::vector<juce::String> noteIds;
    juce::String entryNoteId, exitNoteId;
    juce::File sourceFile;
};
struct NativeSourceEdgePlan
{
    std::vector<NativeSourceEdgeMember> members;
    std::vector<NativeSourceEdgeComponent> components;
};
// Pure, opt-in render-snapshot derivation. Empty selection means all currently
// placed notes. Does not mutate clocks, pitch, source ownership or fade fields.
// Invalid/ambiguous profile ownership is an error, never a nearest-note guess.
bool resolveNativeSourceEdgePlan(const TrackData&, const std::vector<juce::String>& actualNoteIds,
                                 NativeSourceEdgePlan&, juce::String& error);
}
