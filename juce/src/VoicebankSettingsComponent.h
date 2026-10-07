#pragma once

#include "SampleSettings.h"
#include "ModelessWindows.h"
#include "Theme.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>
#include <vector>
#include <optional>

namespace hachi
{
class OtoWaveformEditorComponent;
class VoicebankSettingsComponent final : public juce::Component,
                                         private juce::TableListBoxModel
{
public:
    // jieMode surfaces the four-region (Jie) oto column, its editor and the
    // action that seeds oto4.ini from oto.ini.
    // initialAlias selects the matching entry on open, or the alphabetically
    // nearest one when the voicebank has no such alias.
    VoicebankSettingsComponent(juce::File voicebankRoot, bool jieMode,
                               bool mouMode,
                               juce::String initialAlias = {});
    ~VoicebankSettingsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    std::function<void(OtoWaveformEditorComponent&)> configureOtoEditor;
    juce::DialogWindow* diagnosticEditorWindow() const;
    void diagnosticCollectClosedEditors() { editorWindows.removeClosed(); }
    void diagnosticOpenEditor() { openSelectedEditor(); }
    int diagnosticSelectedRow() const { return table.getSelectedRow(); }
    juce::Point<int> diagnosticScrollPosition() const { return table.getViewport()->getViewPosition(); }
    void diagnosticScrollTo(int x,int y) { table.getViewport()->setViewPosition(x,y); }
    void diagnosticFilter(const juce::String& text) { searchEditor.setText(text);applyFilter(); }
    std::optional<VoicebankOtoEntry> diagnosticSelectedEntry() const {const auto* e=selectedEntry();return e?std::optional<VoicebankOtoEntry>(*e):std::nullopt;}
    void diagnosticReloadFile(const juce::File& file) { reload(file); }
    void diagnosticCreateJieFromMou() { createJieFromMou(); }
    bool diagnosticJieFromMouVisible() const { return createJieFromMouButton.getParentComponent() == this; }

    // Offline checks: the panel is only reachable through a dialog, so the
    // selection and the 2/3/4 buttons are driven directly.
    int diagnosticRowCount() const { return static_cast<int>(filteredRows.size()); }
    void diagnosticSelectRow(int row) { table.selectRow(row); }
    void diagnosticSetRegionCount(int count) { setSelectedRegionCount(count); }
    juce::String diagnosticClasses() const;
    // The count the buttons are showing, or zero when none is on.
    int diagnosticToggledCount() const;

private:
    int getNumRows() override;
    void paintRowBackground(juce::Graphics&, int rowNumber, int width, int height,
                            bool rowIsSelected) override;
    void paintCell(juce::Graphics&, int rowNumber, int columnId, int width, int height,
                   bool rowIsSelected) override;
    void selectedRowsChanged(int lastRowSelected) override;
    void cellDoubleClicked(int rowNumber, int columnId,
                           const juce::MouseEvent& event) override;

    void reload(const juce::File& onlyOtoFile = {});
    void refreshEncodingChoice();
    juce::Label encodingLabel;
    juce::ComboBox encodingFolder, encodingChoice;
    std::vector<juce::File> encodingFolders;
    void applyFilter(std::optional<VoicebankOtoEntry> preferred = {},
                     std::optional<juce::Point<int>> scrollPosition = {});
    void refreshDetails(int filteredRow);
    void openSelectedEditor();
    void duplicateSelectedEntry();
    void createJieOto();
    void createJieFromMou();
    // Switch the selected entry between two, three and four regions.  The
    // panel has no save button, so this writes otomou.ini as it is clicked --
    // the same edit the waveform editor makes, without opening it.
    void setSelectedRegionCount(int count);
    void syncRegionButtons();
    // The entry behind the selected row, or nullptr when nothing is selected.
    const VoicebankOtoEntry* selectedEntry() const;
    // Re-reads the folder and lets the renderer forget what it parsed.
    void refreshFromDisk();
    // Eight classic detail rows, plus the Jie boundaries row.
    std::size_t visibleDetailCount() const { return jie ? 9u : 8u; }
    static juce::String formatMilliseconds(double value);
    static juce::String jieSummary(const VoicebankOtoEntry& entry);

    const bool jie;
    // 谋 mode edits otomou.ini, so the panel opens the waveform editor
    // in the same mode the track is in rather than always in 界.
    const bool mou;
    void createMouOto();
    juce::TextButton createMouButton;
    ModelessWindows editorWindows;
    // Alias to select once the table has been (re)built.
    juce::String pendingAlias;
    juce::File root;
    std::vector<VoicebankOtoEntry> entries;
    std::vector<int> filteredRows;
    juce::StringArray warnings;
    // Audio in the folder that no oto row mentions -- typically a sample
    // dropped in since the voicebank was last written.
    juce::StringArray unregisteredAudio;
    juce::Label rootLabel;
    juce::Label searchLabel;
    juce::TextEditor searchEditor;
    juce::TableListBox table;
    juce::Label detailsTitle;
    std::array<juce::Label, 9> detailLabels;
    std::array<juce::TextEditor, 9> detailEditors;
    juce::Label statusLabel;
    juce::TextButton openEditorButton;
    juce::TextButton duplicateButton;
    juce::TextButton createJieButton;
    juce::TextButton createJieFromMouButton;
    juce::TextButton reloadButton;
    // 2/3/4 段分 for the selected entry, shown only in 谋 mode.
    juce::Label countLabel;
    std::array<juce::TextButton, 3> countButtons;
};
}
