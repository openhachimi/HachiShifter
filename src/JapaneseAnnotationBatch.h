#pragma once
#include "JapaneseAutoAnnotation.h"
#include "OttoMaterialIO.h"
#include <map>

namespace hachi
{
// Producer planning only; publication reuses the one OTTO document writer.
class JapaneseAnnotationBatch final
{
public:
    struct Input
    {
        juce::File root, modelRoot;
        std::vector<juce::File> recordings; // empty: all supported files under root
        backend::OrtExecutionConfig execution;
        juce::StringArray correctedTokens; // alignOnly, current-file correction only
        JapaneseAutoAnnotation::InferenceMode mode = JapaneseAutoAnnotation::InferenceMode::recognizeAndAlign;
        std::map<juce::String,juce::StringArray> manualInputs; // exact full path, never basename/ordinal
        juce::File focusAudio; // explicit visible recording, not a selected alias
    };
    struct Plan
    {
        juce::File root, manifest;
        OttoMaterialIO::Document candidate;
        std::vector<JapaneseAutoAnnotation::Result> results; // aligned, publishable results only
        std::vector<JapaneseAutoAnnotation::Result> recognitions; // normalized stage snapshots, never publishable
        juce::File focusAudio;
        JapaneseAutoAnnotation::InferenceMode mode = JapaneseAutoAnnotation::InferenceMode::recognizeAndAlign;
        JapaneseAutoAnnotation::ModePlan stagePlan;
        int audioLoadCount = 0, asrCallCount = 0, alignmentCallCount = 0;
        juce::String report;
        juce::StringArray failures;
        bool ready = false;
    };
    static bool preview(const Input&, Plan&, juce::String& error,
        const backend::AnnotationCancel& cancel = {},
        const std::function<void(const juce::String&)>& progress = {});
    static bool publish(Plan&, juce::String& error,
        const backend::AnnotationCancel& cancel = {});
};
}
