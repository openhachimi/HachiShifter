#pragma once

#include "../ProjectModel.h"
#include <functional>
#include <optional>

namespace hachi::backend
{
struct MelodyneImportResult
{
    ProjectData project;
    juce::StringArray missingFiles;
    juce::StringArray referencedFiles;
};

struct MelodyneImportOptions
{
    bool recursiveMediaSearch = true;
    bool preserveProjectEdits = true;
};

struct MelodyneConsonantMapping
{
    juce::String consonantNoteId;
    juce::String vowelNoteId;
    double preutteranceSeconds = 0.0;
};

class MelodyneImporter final
{
public:
    // Read-only diagnostics including empty/unresolved tracks, before media
    // filtering. Useful when a named track is absent from an imported project.
    [[nodiscard]] static juce::var inspectTracks(const juce::File& file, juce::String& error);
    using Progress = std::function<void(double, const juce::String&)>;

    [[nodiscard]] static std::optional<MelodyneImportResult>
        importProject(const juce::File& file, juce::String& error, Progress progress = {},
                      MelodyneImportOptions options = {});

    // Conservative mapping used by the later UTAU conversion step.  It only
    // classifies an adjacent note with no voiced pitch samples as an onset;
    // it does not delete or merge source notes in the Melodyne project.
    [[nodiscard]] static std::vector<MelodyneConsonantMapping>
        consonantCandidates(const std::vector<NoteData>& notes);
};
}
