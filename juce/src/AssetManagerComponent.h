#pragma once

#include "I18n.h"
#include "SampleSettings.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <set>
#include <vector>

namespace hachi
{
// The material manager: registered material folders on the left, the audio a
// selected folder holds on the right.  A material folder is an ordinary
// directory of audio plus its HJM parameter sidecars, so a UTAU voicebank, a
// Melodyne-derived folder and a from-scratch folder are all the same thing and
// stay reusable across projects.
//
// The workflow it serves, from nothing: make a folder, drop audio in (the
// system roughly detects parameters at once), edit and save those parameters,
// then drag a member onto a track or type its alias there, and tune.  A folder
// registered once is reused next time.
class AssetManagerComponent final : public juce::Component,
                                    public juce::FileDragAndDropTarget
{
public:
    AssetManagerComponent(I18n& stringsToUse, juce::PropertiesFile& propertiesToUse);
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Test seams: the model without a window, so a check can build a folder,
    // add audio and assemble lyrics without a mouse.
    [[nodiscard]] int diagnosticFolderCount() const
    {
        return static_cast<int>(folders.size());
    }
    [[nodiscard]] juce::StringArray diagnosticMembersOf(int folderIndex) const;
    [[nodiscard]] int diagnosticRegisterFolder(const juce::File& folder);
    // Copy audio into a folder and detect its parameters, exactly as a drop
    // would.  Returns how many files were taken in.
    int diagnosticAddAudioToFolder(int folderIndex, const juce::StringArray& paths);
    // Lyrics -> pinyin -> match the source folder -> a new ordered folder.
    // Returns the created folder path, or empty with the reason in error.
    [[nodiscard]] juce::File diagnosticAssembleLyrics(int sourceFolderIndex,
        const juce::String& lyrics, const juce::String& newFolderName,
        int& matched, int& missing, juce::String& error);
    // Test seam: export the registered folder at this index as one oto.ini.
    int diagnosticExportFolderAsOto(int folderIndex, const juce::File& otoFile,
                                    juce::String& error);
    // Test seam: apply native timing to a member and report success.
    static bool diagnosticWriteNativeTiming(const juce::File& member,
        double offsetMs, double consonantMs, double cutoffMs,
        double preutterMs, double overlapMs, juce::String& error);

private:
    struct MaterialFolder
    {
        juce::File directory;
        juce::String name;
    };

    void load();
    void save();
    int registerFolder(const juce::File& folder, const juce::String& displayName = {});
    void createFolderDialog();
    void importVoicebankDialog();
    void importAudioFolderDialog();
    void addAudioToSelectedFolderDialog();
    void assembleLyricsDialog();
    void exportSelectedFolderAsOto();
    void removeSelectedFolder();
    // Write the selected folder's native parameters back out as one oto.ini,
    // merging every member's rows (oto.ini keys entries by wav name).  The
    // native HJM sidecars stay the material's own store; this is interop.
    // Returns rows written, or -1 with the reason in error.
    int exportFolderAsOto(const juce::File& folder, const juce::File& otoFile,
                          juce::String& error);
    // Copy one audio file into a folder and derive+save its HJM parameters, so
    // parameters exist the moment it is registered.  Returns the landed file.
    juce::File takeAudioIntoFolder(const juce::File& source, const juce::File& folder,
                                   juce::String& error);
    [[nodiscard]] std::vector<juce::File> membersOf(const juce::File& folder) const;
    void rebuildMemberList();
    // Open the native waveform parameter editor on a member: region/alignment/
    // segment editing on the waveform.  A member in an OTO folder round-trips
    // its oto row; a bare from-scratch material saves back to its HJM sidecar.
    void openMemberEditor(const juce::File& member);
    // Write edited timing back to a from-scratch material's native HJM sidecar,
    // never creating an oto.ini.  Shared by the editor's save and the check.
    static bool writeNativeTiming(const juce::File& member,
                                  const VoicebankOtoEntry& edited, juce::String& error);
    // First drag of a member uses the original; a repeat drag hands over a
    // copy, so an external editor that rewrites the file (Melodyne) never
    // touches the registered original.
    [[nodiscard]] juce::File dragFileFor(const juce::File& member);
    [[nodiscard]] juce::File copyRoot() const;
    int folderRowAt(int y) const;
    int memberRowAt(int y) const;
    [[nodiscard]] juce::Rectangle<int> folderPaneBounds() const;
    [[nodiscard]] juce::Rectangle<int> memberPaneBounds() const;

    // Normalised pinyin key of a filename stem for lyric matching:汉字 -> pinyin,
    // then digits and tone marks dropped, lower cased -- so ba / ba2 / bà align.
    [[nodiscard]] static juce::String pinyinMatchKey(const juce::String& stem);

    I18n& strings;
    juce::PropertiesFile& properties;
    juce::TextButton newFolderButton, utauButton, importFolderButton,
                     addAudioButton, assembleButton, exportOtoButton, removeButton;
    std::vector<MaterialFolder> folders;
    std::vector<juce::File> members;
    std::set<juce::String> draggedOnce;
    int selectedFolder = -1;
    int selectedMember = -1;
    int dragStartMember = -1;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::AlertWindow> dialog;
    std::unique_ptr<juce::DialogWindow> editorWindow;
    static constexpr int toolbarHeight = 38;
    static constexpr int rowHeight = 40;
    static constexpr int folderPaneWidth = 220;
};
}
