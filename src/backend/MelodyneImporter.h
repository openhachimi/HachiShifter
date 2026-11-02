#pragma once

#include "../ProjectModel.h"
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace hachi::backend
{
// Read-only provenance for the explicit element synchronization anchor. Target
// coordinates are clip-local seconds; source coordinates are absolute file seconds.
struct MelodyneAlignmentEvidence
{
    juce::String noteId;
    double targetSeconds = 0.0;
    double sourceSeconds = 0.0;
    double supportSourceSeconds = 0.0;
};

struct MelodyneImportResult
{
    ProjectData project;
    juce::StringArray missingFiles;
    juce::StringArray referencedFiles;
    // Original source-continuous join facts, not a transition classification.
    // Graph IDs are scoped to this import; no filename or missing-field guesses.
    std::vector<NativeSplitTransitionEvidence> continuousSourceJoins;
    std::vector<MelodyneAlignmentEvidence> alignmentEvidence;
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

// Small pure diagnostic seam shared by the importer and its support regression
// tests. Points must be sorted by absolute source slice (1024 source samples).
// Support is the actual source item's [startSampleIndex, start + sampleCount],
// expressed in those same slice units, not the sparse property's first/last grid.
namespace melodynePitchSupportDiagnostic
{
struct SourcePitchPoint
{
    double slice = 0.0;
    float raw = 0.0f;
    float smooth = 0.0f;
    bool silent = false;
};
[[nodiscard]] std::optional<std::pair<float, float>>
    pitchAt(const std::vector<SourcePitchPoint>& points, double slice,
            double supportStartSlice, double supportEndSlice);
}

class MelodyneImporter final
{
public:
    // Read-only diagnostics including empty/unresolved tracks, before media
    // filtering. Useful when a named track is absent from an imported project.
    [[nodiscard]] static juce::var inspectTracks(const juce::File& file, juce::String& error);
    // Raw source/element/join facts for comparing partial vs complete cuts.
    // No media resolution/copy, analysis, annotations or import optimization.
    [[nodiscard]] static juce::var inspectJoinGraph(const juce::File& file,juce::String& error);
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
