#pragma once
#include "I18n.h"
#include "Theme.h"

namespace hachi
{
class UtauOutputEnginePanel final : public juce::Component
{
public:
    UtauOutputEnginePanel(I18n& i18n, const juce::String& trackName,
                         const juce::File& resamplerFile, const juce::File& wavtoolFile)
        : strings(i18n)
    {
        setComponentID("track-output-engine-panel");
        heading.setText(trackName, juce::dontSendNotification);
        synthLabel.setText(strings.text("settings.utauWavtool").upToFirstOccurrenceOf("（", false, false).upToFirstOccurrenceOf("(", false, false).trim(), juce::dontSendNotification);
        resamplerLabel.setText(strings.text("settings.utauResampler").upToFirstOccurrenceOf("（", false, false).upToFirstOccurrenceOf("(", false, false).trim(), juce::dontSendNotification);
        help.setText(strings.text("output.toolsHelp"), juce::dontSendNotification);
        help.setColour(juce::Label::textColourId, Palette::textMuted);
        synth.setText(wavtoolFile.getFullPathName(), false);
        resampler.setText(resamplerFile.getFullPathName(), false);
        synth.setComponentID("track-output-wavtool");
        resampler.setComponentID("track-output-resampler");
        apply.setButtonText(strings.text("dialog.apply"));
        cancel.setButtonText(strings.text("dialog.cancel"));
        for (auto* c : std::initializer_list<juce::Component*> { static_cast<juce::Component*>(&heading), &synthLabel, &resamplerLabel, &help,
                        &synth, &resampler, &synthBrowse, &resamplerBrowse, &apply, &cancel })
            addAndMakeVisible(c);
        synthBrowse.onClick = [this] { browse(true); };
        resamplerBrowse.onClick = [this] { browse(false); };
        cancel.onClick = [this] { close(); };
        apply.onClick = [this]
        {
            const juce::File r(resampler.getText().trim().unquoted());
            const juce::File w(synth.getText().trim().unquoted());
            if ((r != juce::File{} && !r.existsAsFile()) || (w != juce::File{} && !w.existsAsFile()))
            {
                help.setText(strings.text("output.invalidTool"), juce::dontSendNotification);
                return;
            }
            if (onApply) onApply(r, w);
            close();
        };
        setSize(800, 260);
    }
    std::function<void(const juce::File&, const juce::File&)> onApply;
    void paint(juce::Graphics& g) override { g.fillAll(Palette::panel); }
    void resized() override
    {
        auto area = getLocalBounds().reduced(20);
        heading.setBounds(area.removeFromTop(28));
        area.removeFromTop(12);
        const auto row = [&](juce::Label& label, juce::TextEditor& editor, juce::TextButton& button)
        {
            auto line = area.removeFromTop(38);
            label.setBounds(line.removeFromLeft(260));
            button.setBounds(line.removeFromRight(42));
            line.removeFromRight(6);
            editor.setBounds(line);
            area.removeFromTop(12);
        };
        row(synthLabel, synth, synthBrowse);
        row(resamplerLabel, resampler, resamplerBrowse);
        help.setBounds(area.removeFromTop(38));
        auto buttons = area.removeFromBottom(34);
        cancel.setBounds(buttons.removeFromRight(110));
        buttons.removeFromRight(12);
        apply.setBounds(buttons.removeFromRight(110));
    }
private:
    void close() { if (auto* window = findParentComponentOfClass<juce::DialogWindow>()) window->exitModalState(0); }
    void browse(bool wavtool)
    {
        chooser = std::make_unique<juce::FileChooser>(strings.text(wavtool ? "settings.chooseUtauWavtool" : "settings.chooseUtauResampler"),
            juce::File((wavtool ? synth : resampler).getText()), "*.exe;*.bat;*.cmd");
        juce::Component::SafePointer<UtauOutputEnginePanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe, wavtool](const juce::FileChooser& chosen)
            {
                if (safe != nullptr && chosen.getResult().existsAsFile())
                    (wavtool ? safe->synth : safe->resampler).setText(chosen.getResult().getFullPathName(), false);
            });
    }
    I18n& strings;
    juce::Label heading, synthLabel, resamplerLabel, help;
    juce::TextEditor synth, resampler;
    juce::TextButton synthBrowse { "..." }, resamplerBrowse { "..." }, apply, cancel;
    std::unique_ptr<juce::FileChooser> chooser;
};
}
