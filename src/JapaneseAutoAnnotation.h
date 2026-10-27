#pragma once
#include "SampleSettings.h"
#include "backend/JapanesePhonemeAsr.h"
#include "backend/TifaJapaneseAligner.h"

namespace hachi
{
class JapaneseAutoAnnotation final
{
public:
    enum class InferenceMode { recognizeAndAlign, recognizeOnly, alignOnly };
    struct ModePlan { bool runRecognition = false, runAlignment = false, canPublish = false; };
    static bool planMode(InferenceMode,ModePlan&,juce::String& error);
    struct Syllable { juce::String alias; int firstPhone = 0, phoneCount = 0, vowelPhone = -1; };
    struct PhoneSequence
    {
        juce::StringArray phones;
        std::vector<std::int64_t> groups;
        std::vector<Syllable> syllables;
    };
    struct Result
    {
        juce::File audio;
        juce::String sourceFingerprint, audioId, asrBackend, alignmentBackend;
        juce::String asrModelFingerprint, alignmentModelFingerprint;
        juce::StringArray recognizedTokens; // raw ASR tokens only; empty in alignOnly
        juce::StringArray normalizedSyllables, manualTokens;
        juce::String dictionaryFingerprint;
        InferenceMode mode = InferenceMode::recognizeAndAlign;
        ModePlan stagePlan;
        int audioLoadCount = 0, asrCallCount = 0, alignmentCallCount = 0;
        bool ready = false; // true only after successful alignment + row validation
        PhoneSequence sequence;
        backend::TifaAlignmentResult alignment;
        std::vector<SampleRegionSetting> rows;
        juce::StringArray warnings;
    };
    static juce::File defaultModelRoot();
    static bool phonemesToSyllables(const juce::StringArray& recognized,
        const juce::File& dictionary, PhoneSequence&, juce::String& error);
    // Explicit mode; no stage is selected merely because manual text exists.
    // recognitionReady receives normalized, unaligned stage data, even if a
    // later TIFA call fails. It never grants publication readiness.
    static bool infer(const juce::File& audio,const juce::File& modelRoot,
        const backend::OrtExecutionConfig&,InferenceMode,const juce::StringArray& manualTokens,
        Result&,juce::String& error,const backend::AnnotationCancel& cancel = {},
        const std::function<void(const juce::String&)>& progress = {},
        const std::function<void(const Result&)>& recognitionReady = {},
        const std::function<void(const Result&)>& stageDiagnostics = {});
    // Legacy wrapper: correctedTokens explicitly selects manual alignment.
    // Preview only. No source, manifest or project mutation occurs here.
    static bool preview(const juce::File& audio, const juce::File& modelRoot,
        const backend::OrtExecutionConfig&, Result&, juce::String& error,
        const backend::AnnotationCancel& cancel = {},
        const std::function<void(const juce::String&)>& progress = {},
        const juce::StringArray& correctedTokens = {});
};
}
