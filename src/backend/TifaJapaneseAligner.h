#pragma once
#include "JapaneseAnnotationAudio.h"
#include "OrtExecution.h"
#include <cstdint>

namespace hachi::backend
{
struct TifaPhoneSpan
{
    juce::String phone;
    double startSeconds = 0, endSeconds = 0;
    double cosineAgreement = 0;
};
struct TifaAlignmentResult
{
    std::vector<TifaPhoneSpan> phones;
    juce::String activeBackend, modelFingerprint;
    double timestep = 0;
    int skippedPhones = 0;
};
// Known-phone alignment. Dictionary/G2P and publication stay outside this engine.
class TifaJapaneseAligner final
{
public:
    static bool align(const AnnotationAudio&, const juce::StringArray& phones,
        const std::vector<std::int64_t>& groups, const juce::File& modelDirectory,
        const OrtExecutionConfig&, TifaAlignmentResult&, juce::String& error,
        const AnnotationCancel& cancel = {});
};
}
