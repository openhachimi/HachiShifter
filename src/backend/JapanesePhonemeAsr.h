#pragma once
#include "JapaneseAnnotationAudio.h"
#include "OrtExecution.h"

namespace hachi::backend
{
struct JapaneseAsrResult
{
    juce::StringArray tokens;
    juce::String activeBackend, modelFingerprint;
    std::size_t frames = 0;
};
// Phoneme/mora recognition only. CTC run positions are NOT forced alignment.
class JapanesePhonemeAsr final
{
public:
    static bool transcribe(const AnnotationAudio&, const juce::File& modelDirectory,
        const OrtExecutionConfig&, JapaneseAsrResult&, juce::String& error,
        const AnnotationCancel& cancel = {});
};
}
