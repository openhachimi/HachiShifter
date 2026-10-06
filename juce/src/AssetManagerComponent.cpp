#include "AssetManagerComponent.h"
#include "OtoWaveformEditorComponent.h"
#include "Pinyin.h"
#include "Theme.h"
#include "backend/UtauRenderer.h"
#include <algorithm>
#include <juce_audio_formats/juce_audio_formats.h>

namespace hachi
{
namespace
{
constexpr const char* audioExtensions = "wav;flac;aif;aiff;mp3;ogg";

double probeDurationSeconds(const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0) return 0.0;
    return static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
}
}

AssetManagerComponent::AssetManagerComponent(I18n& stringsToUse,
                                             juce::PropertiesFile& propertiesToUse)
    : strings(stringsToUse), properties(propertiesToUse)
{
    for (auto* button : { &newFolderButton, &utauButton, &importFolderButton,
                          &addAudioButton, &assembleButton, &exportOtoButton,
                          &removeButton })
        addAndMakeVisible(*button);
    newFolderButton.setButtonText(strings.text("asset.newFolder"));
    utauButton.setButtonText(strings.text("asset.utau"));
    importFolderButton.setButtonText(strings.text("asset.importFolder"));
    addAudioButton.setButtonText(strings.text("asset.addAudio"));
    assembleButton.setButtonText(strings.text("asset.assemble"));
    exportOtoButton.setButtonText(strings.text("asset.exportOto"));
    removeButton.setButtonText(strings.text("asset.remove"));

    newFolderButton.onClick = [this] { createFolderDialog(); };
    utauButton.onClick = [this] { importVoicebankDialog(); };
    importFolderButton.onClick = [this] { importAudioFolderDialog(); };
    addAudioButton.onClick = [this] { addAudioToSelectedFolderDialog(); };
    assembleButton.onClick = [this] { assembleLyricsDialog(); };
    exportOtoButton.onClick = [this] { exportSelectedFolderAsOto(); };
    removeButton.onClick = [this] { removeSelectedFolder(); };

    load();
    setSize(720, 540);
}

// ---------------------------------------------------------------------------
// Persistence

void AssetManagerComponent::load()
{
    folders.clear();
    // New folder-based registry.  A path per line, an optional display name
    // after a tab so a renamed folder keeps its label.
    const auto stored = juce::StringArray::fromLines(
        properties.getValue("assets.materialFolders"));
    for (const auto& line : stored)
    {
        if (line.trim().isEmpty()) continue;
        const auto tab = line.indexOfChar('\t');
        const auto path = tab >= 0 ? line.substring(0, tab) : line;
        const auto name = tab >= 0 ? line.substring(tab + 1) : juce::String();
        const juce::File directory(path.trim());
        if (directory.isDirectory())
            folders.push_back({ directory,
                name.trim().isNotEmpty() ? name.trim() : directory.getFileName() });
    }

    // Migrate the old flat file list: each registered file's parent folder
    // becomes a material folder, so nothing already registered is lost.
    const auto legacy = juce::StringArray::fromLines(properties.getValue("assets.files"));
    for (const auto& path : legacy)
    {
        const juce::File file(path.trim());
        if (file.existsAsFile())
        {
            const auto parent = file.getParentDirectory();
            const auto known = std::any_of(folders.begin(), folders.end(),
                [&](const auto& folder)
                {
                    return folder.directory.getFullPathName()
                        == parent.getFullPathName();
                });
            if (!known && parent.isDirectory())
                folders.push_back({ parent, parent.getFileName() });
        }
    }
    if (!legacy.isEmpty())
    {
        properties.removeValue("assets.files");
        save();
    }

    selectedFolder = folders.empty() ? -1 : 0;
    rebuildMemberList();
}

void AssetManagerComponent::save()
{
    juce::StringArray lines;
    for (const auto& folder : folders)
        lines.add(folder.directory.getFullPathName() + "\t" + folder.name);
    properties.setValue("assets.materialFolders", lines.joinIntoString("\n"));
    properties.saveIfNeeded();
}

int AssetManagerComponent::registerFolder(const juce::File& folder,
                                          const juce::String& displayName)
{
    if (!folder.isDirectory()) return -1;
    for (std::size_t index = 0; index < folders.size(); ++index)
        if (folders[index].directory.getFullPathName() == folder.getFullPathName())
        {
            selectedFolder = static_cast<int>(index);
            rebuildMemberList();
            return selectedFolder;
        }
    folders.push_back({ folder,
        displayName.isNotEmpty() ? displayName : folder.getFileName() });
    save();
    selectedFolder = static_cast<int>(folders.size()) - 1;
    rebuildMemberList();
    return selectedFolder;
}

// ---------------------------------------------------------------------------
// Members and auto-detection

std::vector<juce::File> AssetManagerComponent::membersOf(const juce::File& folder) const
{
    std::vector<juce::File> result;
    if (!folder.isDirectory()) return result;
    juce::Array<juce::File> found;
    folder.findChildFiles(found, juce::File::findFiles, true, "*");
    for (const auto& file : found)
        if (file.hasFileExtension(audioExtensions))
            result.push_back(file);
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right)
    {
        return left.getFullPathName().compareNatural(right.getFullPathName()) < 0;
    });
    return result;
}

void AssetManagerComponent::rebuildMemberList()
{
    members.clear();
    selectedMember = -1;
    if (selectedFolder >= 0 && selectedFolder < static_cast<int>(folders.size()))
        members = membersOf(folders[static_cast<std::size_t>(selectedFolder)].directory);
    repaint();
}

juce::File AssetManagerComponent::takeAudioIntoFolder(const juce::File& source,
                                                      const juce::File& folder,
                                                      juce::String& error)
{
    if (!source.existsAsFile() || !source.hasFileExtension(audioExtensions))
    {
        error = source.getFileName() + ": not an audio file";
        return {};
    }
    if (!folder.isDirectory() && !folder.createDirectory())
    {
        error = "Could not create " + folder.getFullPathName();
        return {};
    }
    auto destination = folder.getChildFile(source.getFileName());
    // Register-in-place when the audio is already inside the folder; otherwise
    // copy it in so the material folder is self-contained and reusable.
    if (source.getParentDirectory().getFullPathName() != folder.getFullPathName())
    {
        destination = folder.getNonexistentChildFile(
            source.getFileNameWithoutExtension(), source.getFileExtension(), false);
        if (!source.copyFileTo(destination))
        {
            error = "Could not copy " + source.getFileName();
            return {};
        }
    }
    // Reading an OTO bank must not write HJM: OTO stays authoritative and is
    // converted to an equivalent native annotation in memory at load time.
    // Only genuinely bare audio -- a from-scratch material with no oto -- gets
    // its parameters roughly detected and saved as a native HJM sidecar here.
    const auto governedByOto = destination.getParentDirectory()
            .getChildFile("oto.ini").existsAsFile()
        || destination.getParentDirectory().getChildFile("oto.jie.ini").existsAsFile()
        || destination.getParentDirectory().getChildFile("otomou.ini").existsAsFile();
    if (!governedByOto && !SampleSettings::sidecarFor(destination).existsAsFile())
    {
        const auto duration = probeDurationSeconds(destination);
        std::vector<SampleRegionSetting> rows;
        SampleRegionSetting row;
        row.name = destination.getFileNameWithoutExtension();
        row.regionStartSeconds = 0.0;
        row.regionEndSeconds = duration > 0.001 ? duration : 0.5;
        // A short lead-in as a first estimate of the consonant/attack region.
        row.fixedDurationSeconds = std::min(0.05, row.regionEndSeconds * 0.25);
        row.alignmentSeconds = row.fixedDurationSeconds;
        row.provenance = "estimated";
        rows.push_back(std::move(row));
        juce::String saveError;
        if (!SampleSettings::save(destination, rows, saveError))
        {
            error = saveError;
            return destination; // The audio is in; only its sidecar failed.
        }
    }
    return destination;
}

// ---------------------------------------------------------------------------
// Toolbar actions

void AssetManagerComponent::createFolderDialog()
{
    chooser = std::make_unique<juce::FileChooser>(
        strings.text("asset.newFolder"), juce::File{});
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& chosen)
        {
            auto folder = chosen.getResult();
            if (folder == juce::File{}) return;
            if (!folder.isDirectory() && !folder.createDirectory())
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    strings.text("asset.newFolder"),
                    "Could not create " + folder.getFullPathName());
                return;
            }
            registerFolder(folder);
        });
}

void AssetManagerComponent::importVoicebankDialog()
{
    chooser = std::make_unique<juce::FileChooser>(strings.text("asset.utau"), juce::File{});
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& chosen)
        {
            const auto directory = chosen.getResult();
            if (!directory.isDirectory()) return;
            // A UTAU voicebank is already a material folder: register it as one
            // and read its oto so its members carry parameters immediately.
            juce::StringArray imported, warnings;
            int sidecars = 0, regions = 0;
            SampleSettings::importVoicebank(directory, imported, sidecars, regions, warnings);
            registerFolder(directory);
            auto message = strings.text("asset.utauDone")
                .replace("{files}", juce::String(imported.size()))
                .replace("{sidecars}", juce::String(sidecars))
                .replace("{regions}", juce::String(regions));
            if (!warnings.isEmpty()) message += "\n\n" + warnings.joinIntoString("\n");
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                strings.text("asset.utau"), message);
        });
}

void AssetManagerComponent::importAudioFolderDialog()
{
    chooser = std::make_unique<juce::FileChooser>(
        strings.text("asset.importFolder"), juce::File{});
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
        [this](const juce::FileChooser& chosen)
        {
            const auto directory = chosen.getResult();
            if (!directory.isDirectory()) return;
            const auto index = registerFolder(directory);
            // takeAudioIntoFolder writes an HJM sidecar only for bare audio in
            // a folder with no oto, so a voicebank folder read here keeps its
            // OTO authoritative and unwritten while a plain audio folder still
            // gets its from-scratch parameters detected.
            if (index >= 0)
                for (const auto& member : membersOf(directory))
                    if (!SampleSettings::sidecarFor(member).existsAsFile())
                    {
                        juce::String error;
                        takeAudioIntoFolder(member, directory, error);
                    }
            rebuildMemberList();
        });
}

void AssetManagerComponent::addAudioToSelectedFolderDialog()
{
    if (selectedFolder < 0 || selectedFolder >= static_cast<int>(folders.size()))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            strings.text("asset.addAudio"), strings.text("asset.pickFolderFirst"));
        return;
    }
    const auto folder = folders[static_cast<std::size_t>(selectedFolder)].directory;
    chooser = std::make_unique<juce::FileChooser>(strings.text("asset.addAudio"),
        juce::File{}, juce::String("*.") + juce::String(audioExtensions).replace(";", ";*."));
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles
                             | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, folder](const juce::FileChooser& chosen)
        {
            juce::StringArray failures;
            for (const auto& file : chosen.getResults())
            {
                juce::String error;
                takeAudioIntoFolder(file, folder, error);
                if (error.isNotEmpty()) failures.add(error);
            }
            rebuildMemberList();
            if (!failures.isEmpty())
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    strings.text("asset.addAudio"), failures.joinIntoString("\n"));
        });
}

// ---------------------------------------------------------------------------
// 活字印刷: lyrics -> pinyin -> match a source folder -> a new ordered folder

juce::String AssetManagerComponent::pinyinMatchKey(const juce::String& stem)
{
    // The filename may already be pinyin, or a Chinese character, or carry a
    // tone digit / diacritic.  Convert any Chinese first, then strip tone
    // digits and lower case so ba / ba2 / bà all reduce to the same key.
    auto key = lyricInPinyin(stem).toLowerCase();
    juce::String cleaned;
    for (auto character : key)
        if (!juce::CharacterFunctions::isDigit(static_cast<juce::juce_wchar>(character)))
            cleaned += juce::String::charToString(character);
    return cleaned.trim();
}

juce::File AssetManagerComponent::diagnosticAssembleLyrics(int sourceFolderIndex,
    const juce::String& lyrics, const juce::String& newFolderName,
    int& matched, int& missing, juce::String& error)
{
    matched = 0;
    missing = 0;
    if (sourceFolderIndex < 0 || sourceFolderIndex >= static_cast<int>(folders.size()))
    {
        error = "no source folder";
        return {};
    }
    const auto source = folders[static_cast<std::size_t>(sourceFolderIndex)].directory;
    const auto sourceMembers = membersOf(source);
    if (sourceMembers.empty())
    {
        error = "source folder has no audio";
        return {};
    }

    // Build a lookup from normalised pinyin key to the first member that has it.
    // Chinese characters are matched directly too, so a bank named in 汉字 works.
    std::map<juce::String, juce::File> byKey;
    for (const auto& member : sourceMembers)
    {
        const auto stem = member.getFileNameWithoutExtension();
        const auto key = pinyinMatchKey(stem);
        if (key.isNotEmpty()) byKey.emplace(key, member);
    }

    const auto name = newFolderName.trim().isNotEmpty()
        ? newFolderName.trim() : juce::String("lyrics");
    const auto destination = copyRoot().getChildFile("lyrics-assemblies")
        .getChildFile(name);
    if (!destination.deleteRecursively() || !destination.createDirectory())
    {
        error = "could not create " + destination.getFullPathName();
        return {};
    }

    auto ordinal = 0;
    for (auto character : lyrics)
    {
        const auto wide = static_cast<juce::juce_wchar>(character);
        if (juce::CharacterFunctions::isWhitespace(wide)) continue;
        const auto single = juce::String::charToString(wide);
        const auto key = pinyinMatchKey(single);
        if (key.isEmpty()) continue;
        ++ordinal;
        const auto found = byKey.find(key);
        if (found == byKey.end()) { ++missing; continue; }
        const auto ordinalText = juce::String(ordinal).paddedLeft('0', 3);
        const auto target = destination.getChildFile(
            ordinalText + "_" + single + "_" + key + found->second.getFileExtension());
        if (found->second.copyFileTo(target))
        {
            // Carry the material's detected parameters with the copy.
            const auto sidecar = SampleSettings::sidecarFor(found->second);
            if (sidecar.existsAsFile())
                sidecar.copyFileTo(SampleSettings::sidecarFor(target));
            ++matched;
        }
        else ++missing;
    }
    if (matched == 0)
    {
        error = "no lyric matched the source folder";
        destination.deleteRecursively();
        return {};
    }
    return destination;
}

void AssetManagerComponent::assembleLyricsDialog()
{
    if (selectedFolder < 0 || selectedFolder >= static_cast<int>(folders.size()))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            strings.text("asset.assemble"), strings.text("asset.pickFolderFirst"));
        return;
    }
    const auto sourceIndex = selectedFolder;
    dialog = std::make_unique<juce::AlertWindow>(strings.text("asset.assemble"),
        strings.text("asset.assembleHint"), juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("name", "", strings.text("asset.assembleName"));
    dialog->addTextEditor("lyrics", "", strings.text("asset.assembleLyrics"), true);
    if (auto* editor = dialog->getTextEditor("lyrics"))
    {
        editor->setMultiLine(true);
        editor->setReturnKeyStartsNewLine(true);
        editor->setSize(360, 120);
    }
    dialog->addButton(strings.text("dialog.apply"), 1, juce::KeyPress(juce::KeyPress::returnKey));
    dialog->addButton(strings.text("dialog.cancel"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<AssetManagerComponent> safe(this);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe, sourceIndex](int result)
        {
            if (safe == nullptr) return;
            auto owned = std::move(safe->dialog);
            if (result != 1 || owned == nullptr) return;
            const auto name = owned->getTextEditorContents("name");
            const auto lyrics = owned->getTextEditorContents("lyrics");
            int matched = 0, missing = 0;
            juce::String error;
            const auto created = safe->diagnosticAssembleLyrics(sourceIndex, lyrics,
                name, matched, missing, error);
            if (created == juce::File{})
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    safe->strings.text("asset.assemble"), error);
                return;
            }
            safe->registerFolder(created);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                safe->strings.text("asset.assemble"),
                safe->strings.text("asset.assembleDone")
                    .replace("{matched}", juce::String(matched))
                    .replace("{missing}", juce::String(missing)));
        }), false);
}

int AssetManagerComponent::exportFolderAsOto(const juce::File& folder,
                                             const juce::File& otoFile,
                                             juce::String& error)
{
    const auto folderMembers = membersOf(folder);
    if (folderMembers.empty())
    {
        error = "no audio in folder";
        return -1;
    }
    // oto.ini keys entries by wav name, so every member's native rows are
    // merged into one file.  The native HJM sidecars stay the material's own
    // store; this is an explicit export for interop, not a format switch.
    juce::String output;
    auto rowsWritten = 0;
    for (const auto& member : folderMembers)
    {
        const auto rows = SampleSettings::loadOrDerive(member, ProjectData{});
        const auto duration = probeDurationSeconds(member);
        for (const auto& row : rows)
        {
            const auto cutoff = std::max(0.0, duration - row.regionEndSeconds) * 1000.0;
            output += member.getFileName() + "="
                + (row.name.isNotEmpty() ? row.name : member.getFileNameWithoutExtension())
                + "," + juce::String(row.regionStartSeconds * 1000.0, 3)
                + "," + juce::String(row.fixedDurationSeconds * 1000.0, 3)
                + "," + juce::String(cutoff, 3)
                + "," + juce::String((row.alignmentSeconds - row.regionStartSeconds) * 1000.0, 3)
                + "," + juce::String(row.overlapSeconds * 1000.0, 3) + "\n";
            ++rowsWritten;
        }
    }
    if (!otoFile.replaceWithText(output, false, false, "\n"))
    {
        error = "Could not write " + otoFile.getFullPathName();
        return -1;
    }
    return rowsWritten;
}

int AssetManagerComponent::diagnosticExportFolderAsOto(int folderIndex,
    const juce::File& otoFile, juce::String& error)
{
    if (folderIndex < 0 || folderIndex >= static_cast<int>(folders.size()))
    {
        error = "no such folder";
        return -1;
    }
    return exportFolderAsOto(folders[static_cast<std::size_t>(folderIndex)].directory,
                             otoFile, error);
}

void AssetManagerComponent::exportSelectedFolderAsOto()
{
    if (selectedFolder < 0 || selectedFolder >= static_cast<int>(folders.size()))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
            strings.text("asset.exportOto"), strings.text("asset.pickFolderFirst"));
        return;
    }
    const auto folder = folders[static_cast<std::size_t>(selectedFolder)].directory;
    chooser = std::make_unique<juce::FileChooser>(strings.text("asset.exportOto"),
        folder.getChildFile("oto.ini"), "*.ini");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode
                             | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, folder](const juce::FileChooser& chosen)
        {
            auto file = chosen.getResult();
            if (file == juce::File{}) return;
            if (!file.hasFileExtension("ini")) file = file.withFileExtension("ini");
            juce::String error;
            const auto rows = exportFolderAsOto(folder, file, error);
            if (rows < 0)
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    strings.text("asset.exportOto"), error);
            else
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                    strings.text("asset.exportOto"),
                    strings.text("asset.exportOtoDone")
                        .replace("{rows}", juce::String(rows)));
        });
}

void AssetManagerComponent::removeSelectedFolder()
{
    if (selectedFolder < 0 || selectedFolder >= static_cast<int>(folders.size())) return;
    // Only unregister from the manager; the folder on disk is left untouched
    // so a shared voicebank is never deleted from under another project.
    folders.erase(folders.begin() + selectedFolder);
    selectedFolder = std::min(selectedFolder, static_cast<int>(folders.size()) - 1);
    save();
    rebuildMemberList();
}

// ---------------------------------------------------------------------------
// Drag out, with copy-on-reuse protection

juce::File AssetManagerComponent::copyRoot() const
{
    return properties.getFile().getParentDirectory().getChildFile("Copy");
}

juce::File AssetManagerComponent::dragFileFor(const juce::File& member)
{
    const auto key = member.getFullPathName();
    if (draggedOnce.find(key) == draggedOnce.end())
    {
        // First drag: the original, and remembered so the next is a copy.
        draggedOnce.insert(key);
        return member;
    }
    // A repeat drag hands over a copy in the user data area, so an external
    // editor that rewrites the file never touches the registered original.
    const auto root = copyRoot();
    if (!root.isDirectory() && !root.createDirectory()) return member;
    const auto copy = root.getNonexistentChildFile(
        member.getFileNameWithoutExtension(), member.getFileExtension(), false);
    if (!member.copyFileTo(copy)) return member;
    const auto sidecar = SampleSettings::sidecarFor(member);
    if (sidecar.existsAsFile()) sidecar.copyFileTo(SampleSettings::sidecarFor(copy));
    return copy;
}

// ---------------------------------------------------------------------------
// Drag in

bool AssetManagerComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return std::any_of(files.begin(), files.end(), [](const auto& path)
    {
        const juce::File file(path);
        return file.isDirectory() || file.hasFileExtension(audioExtensions);
    });
}

void AssetManagerComponent::filesDropped(const juce::StringArray& files, int, int)
{
    juce::StringArray audioPaths;
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (file.isDirectory()) registerFolder(file);
        else if (file.hasFileExtension(audioExtensions)) audioPaths.add(path);
    }
    // Bare audio drops need a folder to live in.  With one selected they join
    // it (and are detected); with none, the parent directory is registered.
    if (!audioPaths.isEmpty())
    {
        if (selectedFolder >= 0 && selectedFolder < static_cast<int>(folders.size()))
        {
            const auto folder = folders[static_cast<std::size_t>(selectedFolder)].directory;
            for (const auto& path : audioPaths)
            {
                juce::String error;
                takeAudioIntoFolder(juce::File(path), folder, error);
            }
        }
        else
        {
            for (const auto& path : audioPaths)
                registerFolder(juce::File(path).getParentDirectory());
        }
    }
    rebuildMemberList();
}

// ---------------------------------------------------------------------------
// Layout and interaction

juce::Rectangle<int> AssetManagerComponent::folderPaneBounds() const
{
    return getLocalBounds().withTrimmedTop(toolbarHeight).withWidth(folderPaneWidth);
}

juce::Rectangle<int> AssetManagerComponent::memberPaneBounds() const
{
    return getLocalBounds().withTrimmedTop(toolbarHeight)
        .withTrimmedLeft(folderPaneWidth + 1);
}

int AssetManagerComponent::folderRowAt(int y) const
{
    const auto row = (y - toolbarHeight) / rowHeight;
    return y >= toolbarHeight && row >= 0 && row < static_cast<int>(folders.size())
        ? row : -1;
}

int AssetManagerComponent::memberRowAt(int y) const
{
    const auto row = (y - toolbarHeight) / rowHeight;
    return y >= toolbarHeight && row >= 0 && row < static_cast<int>(members.size())
        ? row : -1;
}

void AssetManagerComponent::mouseDown(const juce::MouseEvent& event)
{
    dragStartMember = -1;
    if (folderPaneBounds().contains(event.getPosition()))
    {
        const auto row = folderRowAt(event.y);
        if (row >= 0 && row != selectedFolder)
        {
            selectedFolder = row;
            rebuildMemberList();
        }
        selectedFolder = row >= 0 ? row : selectedFolder;
        repaint();
        return;
    }
    if (memberPaneBounds().contains(event.getPosition()))
    {
        selectedMember = memberRowAt(event.y);
        dragStartMember = selectedMember;
        repaint();
    }
}

void AssetManagerComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragStartMember < 0 || dragStartMember >= static_cast<int>(members.size())
        || event.getDistanceFromDragStart() < 6) return;
    const auto member = members[static_cast<std::size_t>(dragStartMember)];
    dragStartMember = -1;
    const juce::StringArray files { dragFileFor(member).getFullPathName() };
    juce::DragAndDropContainer::performExternalDragDropOfFiles(files, false, this);
}

void AssetManagerComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (!memberPaneBounds().contains(event.getPosition())) return;
    const auto row = memberRowAt(event.y);
    if (row < 0 || row >= static_cast<int>(members.size())) return;
    selectedMember = row;
    openMemberEditor(members[static_cast<std::size_t>(row)]);
}

void AssetManagerComponent::openMemberEditor(const juce::File& member)
{
    if (!member.existsAsFile()) return;
    const auto folder = member.getParentDirectory();
    const auto governedByOto = folder.getChildFile("oto.ini").existsAsFile()
        || folder.getChildFile("oto.jie.ini").existsAsFile()
        || folder.getChildFile("otomou.ini").existsAsFile();

    VoicebankOtoEntry entry;
    std::function<bool(const VoicebankOtoEntry&, juce::String&)> saveOverride;

    if (governedByOto)
    {
        // An OTO material: edit its real oto row so the change round-trips OTO.
        juce::StringArray warnings;
        const auto entries = SampleSettings::loadVoicebankOto(folder, warnings);
        auto match = std::find_if(entries.begin(), entries.end(),
            [&](const auto& candidate)
            {
                return candidate.audioFile.getFullPathName() == member.getFullPathName();
            });
        if (match == entries.end())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                strings.text("asset.editTitle"), strings.text("asset.editNoOto"));
            return;
        }
        entry = *match;
    }
    else
    {
        // A from-scratch material: derive an entry from its native HJM row and
        // save changes back to the sidecar, never creating an oto.ini.
        const auto rows = SampleSettings::loadOrDerive(member, ProjectData{});
        if (rows.empty()) return;
        const auto& row = rows.front();
        const auto duration = probeDurationSeconds(member);
        entry.audioFile = member;
        entry.sourceName = member.getFileName();
        entry.alias = row.name;
        entry.offsetMs = row.regionStartSeconds * 1000.0;
        entry.consonantMs = row.fixedDurationSeconds * 1000.0;
        entry.preutteranceMs = (row.alignmentSeconds - row.regionStartSeconds) * 1000.0;
        entry.overlapMs = row.overlapSeconds * 1000.0;
        // Positive cutoff = trim from the file end, mirroring importOto.
        entry.cutoffMs = duration > 0.0
            ? (duration - row.regionEndSeconds) * 1000.0 : 0.0;
        entry.lineIndex = -1;
        saveOverride = [member](const VoicebankOtoEntry& edited,
                                juce::String& error) -> bool
        {
            return writeNativeTiming(member, edited, error);
        };
    }

    auto editor = std::make_unique<OtoWaveformEditorComponent>(entry, false, false,
        [this] { rebuildMemberList(); });
    if (saveOverride) editor->setSaveOverride(std::move(saveOverride));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = strings.text("asset.editTitle") + " — " + member.getFileName();
    options.dialogBackgroundColour = Palette::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.content.setOwned(editor.release());
    if (auto* window = options.launchAsync())
        window->setResizeLimits(720, 400, 1800, 1100);
}

bool AssetManagerComponent::writeNativeTiming(const juce::File& member,
    const VoicebankOtoEntry& edited, juce::String& error)
{
    const auto duration = probeDurationSeconds(member);
    auto stored = SampleSettings::loadOrDerive(member, ProjectData{});
    if (stored.empty()) stored.push_back({});
    auto& target = stored.front();
    target.name = edited.alias.isNotEmpty() ? edited.alias
                                            : member.getFileNameWithoutExtension();
    target.regionStartSeconds = edited.offsetMs / 1000.0;
    target.fixedDurationSeconds = edited.consonantMs / 1000.0;
    target.alignmentSeconds = target.regionStartSeconds + edited.preutteranceMs / 1000.0;
    target.overlapSeconds = edited.overlapMs / 1000.0;
    // Same cutoff convention as importOto: negative is a length from offset,
    // positive trims that amount from the physical file end.
    target.regionEndSeconds = edited.cutoffMs < 0.0
        ? target.regionStartSeconds + (-edited.cutoffMs) / 1000.0
        : duration > 0.0 ? duration - edited.cutoffMs / 1000.0
                         : target.regionStartSeconds + 0.5;
    target.regionEndSeconds = std::max(target.regionStartSeconds + 0.001,
                                       target.regionEndSeconds);
    target.provenance = "native";
    return SampleSettings::save(member, stored, error);
}

bool AssetManagerComponent::diagnosticWriteNativeTiming(const juce::File& member,
    double offsetMs, double consonantMs, double cutoffMs,
    double preutterMs, double overlapMs, juce::String& error)
{
    VoicebankOtoEntry edited;
    edited.audioFile = member;
    edited.alias = member.getFileNameWithoutExtension();
    edited.offsetMs = offsetMs;
    edited.consonantMs = consonantMs;
    edited.cutoffMs = cutoffMs;
    edited.preutteranceMs = preutterMs;
    edited.overlapMs = overlapMs;
    return writeNativeTiming(member, edited, error);
}

void AssetManagerComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::panel);
    g.setColour(Palette::border);
    g.drawHorizontalLine(toolbarHeight - 1, 0.0f, static_cast<float>(getWidth()));
    g.drawVerticalLine(folderPaneWidth, static_cast<float>(toolbarHeight),
                       static_cast<float>(getHeight()));

    // Folder pane.
    if (folders.empty())
    {
        g.setColour(Palette::textMuted);
        g.drawFittedText(strings.text("asset.empty"),
            folderPaneBounds().reduced(16), juce::Justification::centred, 4);
    }
    for (int index = 0; index < static_cast<int>(folders.size()); ++index)
    {
        auto row = juce::Rectangle<int>(0, toolbarHeight + index * rowHeight,
                                        folderPaneWidth, rowHeight);
        g.setColour(index == selectedFolder ? Palette::accent.withAlpha(0.22f)
                                            : index % 2 == 0 ? Palette::panel
                                                             : Palette::panelRaised);
        g.fillRect(row);
        g.setColour(Palette::accentLight);
        g.fillRoundedRectangle(row.removeFromLeft(9).reduced(3).toFloat(), 2.0f);
        g.setColour(Palette::text);
        g.drawText(folders[static_cast<std::size_t>(index)].name,
                   row.reduced(8, 2).removeFromTop(20),
                   juce::Justification::centredLeft, true);
        g.setColour(Palette::textMuted);
        g.setFont(9.5f);
        g.drawText(folders[static_cast<std::size_t>(index)].directory.getFullPathName(),
                   row.reduced(8, 2), juce::Justification::centredLeft, true);
    }

    // Member pane.
    const auto pane = memberPaneBounds();
    if (selectedFolder < 0)
        return;
    if (members.empty())
    {
        g.setColour(Palette::textMuted);
        g.drawFittedText(strings.text("asset.folderEmpty"), pane.reduced(20),
                         juce::Justification::centred, 3);
        return;
    }
    for (int index = 0; index < static_cast<int>(members.size()); ++index)
    {
        auto row = juce::Rectangle<int>(pane.getX(), toolbarHeight + index * rowHeight,
                                        pane.getWidth(), rowHeight);
        g.setColour(index == selectedMember ? Palette::accent.withAlpha(0.2f)
                                           : index % 2 == 0 ? Palette::panel
                                                            : Palette::panelRaised);
        g.fillRect(row);
        const auto& member = members[static_cast<std::size_t>(index)];
        const auto hasParams = SampleSettings::sidecarFor(member).existsAsFile()
            || juce::File(member.getFullPathName() + ".hachi.csv").existsAsFile();
        g.setColour(hasParams ? Palette::noteFill : Palette::textMuted);
        g.fillRoundedRectangle(row.removeFromLeft(9).reduced(3).toFloat(), 2.0f);
        g.setColour(Palette::text);
        g.drawText(member.getFileName(), row.reduced(9, 2).removeFromTop(22),
                   juce::Justification::centredLeft, true);
        g.setColour(Palette::textMuted);
        g.setFont(9.5f);
        g.drawText(hasParams ? strings.text("asset.paramsReady")
                             : strings.text("asset.paramsNone"),
                   row.reduced(9, 2), juce::Justification::centredLeft, true);
    }
}

void AssetManagerComponent::resized()
{
    auto toolbar = getLocalBounds().removeFromTop(toolbarHeight).reduced(5, 5);
    const auto take = [&toolbar](juce::Button& button, int width)
    {
        button.setBounds(toolbar.removeFromLeft(width));
        toolbar.removeFromLeft(4);
    };
    take(newFolderButton, 88);
    take(utauButton, 120);
    take(importFolderButton, 88);
    take(addAudioButton, 84);
    take(assembleButton, 84);
    take(exportOtoButton, 96);
    take(removeButton, 64);
}

// ---------------------------------------------------------------------------
// Test seams

juce::StringArray AssetManagerComponent::diagnosticMembersOf(int folderIndex) const
{
    juce::StringArray names;
    if (folderIndex < 0 || folderIndex >= static_cast<int>(folders.size())) return names;
    for (const auto& member : membersOf(folders[static_cast<std::size_t>(folderIndex)].directory))
        names.add(member.getFileName());
    return names;
}

int AssetManagerComponent::diagnosticRegisterFolder(const juce::File& folder)
{
    return registerFolder(folder);
}

int AssetManagerComponent::diagnosticAddAudioToFolder(int folderIndex,
                                                      const juce::StringArray& paths)
{
    if (folderIndex < 0 || folderIndex >= static_cast<int>(folders.size())) return 0;
    const auto folder = folders[static_cast<std::size_t>(folderIndex)].directory;
    auto added = 0;
    for (const auto& path : paths)
    {
        juce::String error;
        const auto landed = takeAudioIntoFolder(juce::File(path), folder, error);
        if (landed != juce::File{}) ++added;
    }
    rebuildMemberList();
    return added;
}
}
