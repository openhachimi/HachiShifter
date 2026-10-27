#pragma once
#include "JapaneseAnnotationBatch.h"
#include "I18n.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>

namespace hachi
{
// A preview/publication wizard, not an annotation editor. All inference,
// planning and OTTO writes use JapaneseAnnotationBatch's shared authority.
class JapaneseAutoAnnotationDialog final : public juce::Component
{
public:
    struct Host
    {
        // Required caller-owned off-message-thread executor. Accepted jobs must
        // execute once and drain before message-loop/application shutdown.
        std::function<void(std::function<void()>)> runOnWorker;
        // Message-thread, nonthrowing callbacks. Own/guard the host lifetime.
        // Gate true precedes preflight; false follows completion even if this
        // widget has been deleted. published only follows actual disk success.
        std::function<void(bool)> publishing;
        std::function<bool(const JapaneseAnnotationBatch::Plan&, juce::String&)> preflight;
        std::function<void(const JapaneseAnnotationBatch::Plan&)> published;
    };
    struct Input
    {
        juce::File selectedFolderRoot;
        juce::File modelRoot = JapaneseAutoAnnotation::defaultModelRoot();
        juce::File currentAudio; // empty: recurse supported files in selectedFolderRoot
        juce::StringArray correctedTokens; // space-separated romaji; current file only
        backend::OrtExecutionConfig execution { backend::InferenceBackend::cpu, -1, 0 };
    };
    JapaneseAutoAnnotationDialog(I18n&, Host, Input initial);
    JapaneseAutoAnnotationDialog(I18n&, Host);
    ~JapaneseAutoAnnotationDialog() override;
    void resized() override;
    void paint(juce::Graphics&) override;
    bool setInput(const Input&, juce::String& error);
    using InferenceMode = JapaneseAutoAnnotation::InferenceMode;
    void infer(InferenceMode);
    void preview(); // legacy seam: full recognition + alignment
    juce::String manualInputText() const;
    void setManualInputText(const juce::String&, bool notify = true);
    juce::File selectedManualAudio() const;
    bool selectManualAudio(const juce::File&);
    void showPublishConfirmation(); // same production confirmation action
    bool publishConfirmed(juce::String& error); // caller explicitly confirmed
    bool previewReady() const;
    const JapaneseAnnotationBatch::Plan* previewPlan() const;
    juce::String reportText() const;
    bool busy() const;
    bool publicationInFlight() const;
    void cancel();
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
}
