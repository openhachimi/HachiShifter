#pragma once

#include <juce_core/juce_core.h>

namespace hachi::backend
{
// One note's own oto entry, edited for that note alone.
//
// The same numbers an oto row holds, in the same units -- milliseconds, with
// the cutoff signed the way oto.ini signs it and the three inner boundaries
// measured from the offset -- so the editor that writes a row can write one of
// these instead.  When it is enabled the note renders from it in place of its
// voicebank entry, and the oto files are never touched.  The recording itself
// still comes from the entry the note's lyric resolves to.
//
// Header-only and free of both the project model and the renderer, because
// both of them hold one.
struct UtauOtoOverride
{
    bool enabled = false;
    double offsetMs = 0.0;
    double consonantMs = 0.0;
    double cutoffMs = 0.0;
    double preutteranceMs = 0.0;
    double overlapMs = 0.0;
    // The four-region boundaries, used by 界 and 谋.  hasRegions is false for
    // an entry with none, which then behaves as a classic two-region entry.
    bool hasRegions = false;
    double onsetMs = 0.0;
    double glideMs = 0.0;
    double nucleusMs = 0.0;
    // 谋's per-region classes; empty everywhere else.
    juce::String classes;

    [[nodiscard]] bool operator==(const UtauOtoOverride& other) const
    {
        return enabled == other.enabled && offsetMs == other.offsetMs
            && consonantMs == other.consonantMs && cutoffMs == other.cutoffMs
            && preutteranceMs == other.preutteranceMs && overlapMs == other.overlapMs
            && hasRegions == other.hasRegions && onsetMs == other.onsetMs
            && glideMs == other.glideMs && nucleusMs == other.nucleusMs
            && classes == other.classes;
    }
    [[nodiscard]] bool operator!=(const UtauOtoOverride& other) const
    {
        return !(*this == other);
    }
};
}
