#pragma once

#include "I18n.h"
#include "Theme.h"
#include "backend/DiffSingerOptions.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <memory>
#include <vector>

namespace hachi
{
class SettingsComponent final : public juce::Component
{
public:
    using Applied = std::function<void()>;

    SettingsComponent(I18n& stringsToUse, juce::AudioDeviceManager& devicesToUse,
                      juce::PropertiesFile& propertiesToUse, Applied appliedToUse);
    ~SettingsComponent() override;
    void resized() override;
    void selectPage(int page) { tabs.setCurrentTabIndex(page, false); }
    [[nodiscard]] int diagnosticTabCount() const { return tabs.getNumTabs(); }
    [[nodiscard]] int diagnosticInferenceDeviceCount() const
    {
        return inferenceDevice.getNumItems();
    }
    [[nodiscard]] int diagnosticCurrentPageChildCount() const
    {
        if (const auto* page = tabs.getCurrentContentComponent())
            return page->getNumChildComponents();
        return 0;
    }

private:
    class PathPicker final : public juce::Component
    {
    public:
        PathPicker();
        void resized() override;
        void setText(const juce::String& text);
        [[nodiscard]] juce::String getText() const;
        void setBrowseTooltip(const juce::String& text);
        std::function<void()> onBrowse;

    private:
        juce::TextEditor editor;
        juce::TextButton browseButton { "..." };
    };

    class FormPage final : public juce::Component
    {
    public:
        void addRow(juce::Label& label, juce::Component& editor);
        void addWide(juce::Component& component, int height = 28);
        void resized() override;
    private:
        struct Row { juce::Label* label = nullptr; juce::Component* editor = nullptr; int height = 28; };
        std::vector<Row> rows;
    };

    static juce::Colour colourFromText(const juce::String& text, juce::Colour fallback);
    void loadValues();
    void saveValues();
    void refreshDiffSingerControls();
    void setTexts();
    void refreshAudioValues();
    void applyAudioValues();
    void openAdvancedAudioPanel();
    void refreshImportedStretchItems(int preferredId = 0);
    void chooseUtauVoicebank();
    void chooseUtauWavtool();
    void chooseUtauResampler();
    [[nodiscard]] juce::File initialPathFor(const PathPicker& picker,
                                             bool directory) const;

    I18n& strings;
    juce::AudioDeviceManager& devices;
    juce::PropertiesFile& properties;
    Applied onApplied;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    FormPage interfacePage;
    FormPage algorithmPage;
    FormPage operationPage;
    FormPage importPage;
    FormPage audioPage;
    FormPage diffSingerPage;
    juce::Label dsBackendLabel, dsDeviceLabel, dsPreviewLabel, dsExportLabel,
                dsAcousticLabel, dsPitchLabel, dsVarianceLabel, dsDepthLabel, dsHelp, dsStatus;
    juce::ComboBox dsBackend, dsDevice, dsPreview, dsExport;
    juce::Slider dsAcoustic, dsPitch, dsVariance, dsDepth;

    juce::Label languageLabel, themeLabel, accentLabel, accentLightLabel, noteColourLabel;
    juce::ComboBox language, theme;
    juce::TextEditor accent, accentLight, noteColour;
    juce::ToggleButton showNoteLabels, softwareRendering;
    juce::Label uiScaleLabel;
    juce::Slider uiScale;

    juce::Label audioDeviceLabel, sampleRateLabel, bufferSizeLabel, currentAudioDevice;
    juce::ComboBox sampleRate, bufferSize;
    juce::TextButton advancedAudio;

    juce::Label gamePathLabel, gameModelLabel, fcpePathLabel, hifiganPathLabel, inferenceLabel,
                inferenceDeviceLabel, utauVoicebankLabel, utauWavtoolLabel,
                utauResamplerLabel;
    juce::TextEditor gamePath, fcpePath, hifiganPath;
    PathPicker utauVoicebankPath, utauWavtoolPath, utauResamplerPath;
    juce::ComboBox gameModel, inference, inferenceDevice;
    std::unique_ptr<juce::FileChooser> pathChooser;

    juce::Label shortcutLabel;
    juce::ComboBox shortcutPreset;
    juce::ToggleButton spacePlayback, confirmDestructive;

    juce::Label melodyneComposeLabel, melodynePitchLabel, importedAlgorithmLabel,
                importedStretchAlgorithmLabel;
    juce::ComboBox melodyneCompose, melodynePitchSource, importedAlgorithm,
                   importedStretchAlgorithm;
    juce::ToggleButton preserveProjectEdits, locateMediaRecursively;

    juce::TextButton applyButton;
};
}
