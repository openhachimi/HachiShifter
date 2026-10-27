#pragma once

#include "I18n.h"
#include "MelodyneImportOptimization.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace hachi
{
// Header-only so the actual dialog component is also available to GUI diagnostics
// without including MainComponent.cpp or adding a second import implementation.
class MelodyneTrackSelector final : public juce::Component
{
public:
    MelodyneTrackSelector(const std::vector<TrackData>& tracks, const I18n& strings,
                          MelodyneImportOptimizationOptions initialOptions = {})
        : pitchHelp(strings.text("mpd.options.autoPitchHelp")),
          pitchDependencyHelp(strings.text("mpd.options.autoPitchRequiresConnections"))
    {
        setComponentID("melodyne-track-selector");
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(9);
        addAndMakeVisible(viewport);
        for (const auto& track : tracks)
        {
            auto toggle = std::make_unique<juce::ToggleButton>();
            toggle->setButtonText(track.name + juce::String::fromUTF8("  ·  ")
                                  + strings.text(track.compose ? "track.compose" : "track.audio"));
            toggle->setToggleState(track.compose, juce::dontSendNotification);
            content.addAndMakeVisible(*toggle);
            toggles.push_back(std::move(toggle));
        }
        optionsTitle.setText(strings.text("mpd.options.thisImport"), juce::dontSendNotification);
        addAndMakeVisible(optionsTitle);
        configure(registerTransitions, "melodyne-auto-register-transitions", strings.text("mpd.options.autoTransitions"), strings.text("mpd.options.autoTransitionsHelp"));
        configure(virtualConnections, "melodyne-auto-virtual-connections", strings.text("mpd.options.autoConnections"), strings.text("mpd.options.autoConnectionsHelp"));
        configure(virtualPitch, "melodyne-auto-virtual-pitch", strings.text("mpd.options.autoPitch"), pitchHelp);
        configure(mergeConsonants, "melodyne-auto-merge-consonants", strings.text("mpd.options.autoConsonants"), strings.text("mpd.options.autoConsonantsHelp"));
        virtualConnections.onClick = [this] { updateDependencies(); };
        setOptions(initialOptions);
        setSize(560, juce::jlimit(56, 260, static_cast<int>(toggles.size()) * rowHeight + 4) + optionsHeight);
    }

    bool isCompose(std::size_t index) const
    {
        return index < toggles.size() && toggles[index]->getToggleState();
    }

    // These are the real controls used by the modal Apply callback, not a test
    // copy. A disabled pitch choice remains selected and can be re-enabled.
    MelodyneImportOptimizationOptions getOptions() const
    {
        MelodyneImportOptimizationOptions result;
        result.registerTransitions = registerTransitions.getToggleState();
        result.virtualConnections = virtualConnections.getToggleState();
        result.virtualPitch = virtualPitch.getToggleState();
        result.mergeConsonants = mergeConsonants.getToggleState();
        return result;
    }
    void setOptions(const MelodyneImportOptimizationOptions& options)
    {
        registerTransitions.setToggleState(options.registerTransitions, juce::dontSendNotification);
        virtualConnections.setToggleState(options.virtualConnections, juce::dontSendNotification);
        virtualPitch.setToggleState(options.virtualPitch, juce::dontSendNotification);
        mergeConsonants.setToggleState(options.mergeConsonants, juce::dontSendNotification);
        updateDependencies();
    }
    bool isVirtualPitchEnabled() const { return virtualPitch.isEnabled(); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto options = bounds.removeFromBottom(optionsHeight).reduced(6, 0);
        optionsTitle.setBounds(options.removeFromTop(28));
        for (auto* button : { &registerTransitions, &virtualConnections, &virtualPitch, &mergeConsonants })
            button->setBounds(options.removeFromTop(rowHeight));
        viewport.setBounds(bounds);
        const auto contentHeight = std::max(bounds.getHeight(), static_cast<int>(toggles.size()) * rowHeight + 4);
        content.setSize(std::max(1, getWidth() - viewport.getScrollBarThickness()), contentHeight);
        for (std::size_t index = 0; index < toggles.size(); ++index)
            toggles[index]->setBounds(6, 2 + static_cast<int>(index) * rowHeight, content.getWidth() - 12, rowHeight);
    }

private:
    void configure(juce::ToggleButton& button, const char* id, const juce::String& text, const juce::String& help)
    {
        button.setComponentID(id);
        button.setButtonText(text);
        button.setTooltip(help);
        addAndMakeVisible(button);
    }
    void updateDependencies()
    {
        virtualPitch.setEnabled(virtualConnections.getToggleState());
        virtualPitch.setTooltip(virtualConnections.getToggleState() ? pitchHelp : pitchDependencyHelp);
    }
    static constexpr int rowHeight = 28, optionsHeight = 148;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    juce::Label optionsTitle;
    juce::ToggleButton registerTransitions, virtualConnections, virtualPitch, mergeConsonants;
    juce::String pitchHelp, pitchDependencyHelp;
};
}
