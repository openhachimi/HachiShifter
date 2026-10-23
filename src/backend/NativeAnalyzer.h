#pragma once

#include "../ProjectModel.h"
#include <functional>

namespace hachi::backend
{
class NativeAnalyzer final
{
public:
    using Progress = std::function<void(double)>;
    static std::vector<NoteData> analyse(const juce::File& file, juce::String& error,
                                         Progress progress = {});

    // Where a run of unbroken voicing holds more than one syllable.
    //
    // A note only ends here when the pitch tracker goes quiet for longer than
    // 80 ms.  Sung legato there is no such gap, so a whole phrase arrives as
    // one note: on one real recording a 17.8 second vocal came in as 23 notes
    // with the longest running 1.795 seconds over five syllables.
    //
    // The cue that is there is the drop in energy between syllables.  Measured
    // on that recording the joins sat at 5 to 19 per cent of the surrounding
    // peaks, while the dips inside a syllable stayed at 26 per cent and above,
    // so a quarter is the gap between the two.
    //
    // Takes the loudness of one voiced run, frame by frame, and returns the
    // frames to cut at.  Pure, and public so a check reads the same rule the
    // analysis acts on.
    [[nodiscard]] static std::vector<int> syllableCuts(const std::vector<float>& loudness,
                                                       int shortestFrames,
                                                       float valleyDepth);
    static bool reanalyseProjectSourcePitch(ProjectData& project, juce::String& error,
                                            Progress progress = {});
};
}
