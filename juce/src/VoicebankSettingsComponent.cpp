#include "VoicebankSettingsComponent.h"
#include "OtoWaveformEditorComponent.h"
#include "backend/UtauRenderer.h"
#include <algorithm>

namespace hachi
{
namespace
{
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }

constexpr std::array<const char*, 9> detailNames {
    "文件名", "别名", "偏移", "辅音", "截止", "先行声音", "重叠", "oto.ini",
    "界•OTO"
};
}

VoicebankSettingsComponent::VoicebankSettingsComponent(juce::File voicebankRoot,
                                                       bool jieMode,
                                                       bool mouMode,
                                                       juce::String initialAlias)
    : jie(jieMode), mou(mouMode), pendingAlias(std::move(initialAlias)),
      root(std::move(voicebankRoot)), table("oto table", this)
{
    setOpaque(true);
    rootLabel.setText(root.getFullPathName(), juce::dontSendNotification);
    rootLabel.setTooltip(root.getFullPathName());
    rootLabel.setColour(juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible(rootLabel);

    searchLabel.setText(utf8("搜索"), juce::dontSendNotification);
    searchLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(searchLabel);
    searchEditor.setTextToShowWhenEmpty(utf8("文件名、别名或子目录"), Palette::textMuted);
    searchEditor.onTextChange = [this] { applyFilter(); };
    addAndMakeVisible(searchEditor);

    table.setModel(this);
    table.setMultipleSelectionEnabled(false);
    table.setColour(juce::ListBox::backgroundColourId, Palette::graphBackground);
    table.setColour(juce::ListBox::outlineColourId, Palette::border);
    table.setOutlineThickness(1);
    auto& header = table.getHeader();
    header.addColumn(utf8("文件名"), 1, 145, 80, 260);
    header.addColumn(utf8("别名"), 2, 120, 70, 240);
    header.addColumn(utf8("偏移"), 3, 76, 55, 120);
    header.addColumn(utf8("辅音"), 4, 76, 55, 120);
    header.addColumn(utf8("截止"), 5, 76, 55, 120);
    header.addColumn(utf8("先行声音"), 6, 92, 70, 130);
    header.addColumn(utf8("重叠"), 7, 76, 55, 120);
    if (jie) header.addColumn(utf8("界•OTO"), 8, 150, 100, 240);
    addAndMakeVisible(table);

    detailsTitle.setText(utf8("所选条目"), juce::dontSendNotification);
    detailsTitle.setFont(detailsTitle.getFont().boldened());
    addAndMakeVisible(detailsTitle);
    for (std::size_t index = 0; index < visibleDetailCount(); ++index)
    {
        detailLabels[index].setText(utf8(detailNames[index]), juce::dontSendNotification);
        detailLabels[index].setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(detailLabels[index]);
        detailEditors[index].setReadOnly(true);
        detailEditors[index].setCaretVisible(false);
        detailEditors[index].setSelectAllWhenFocused(true);
        addAndMakeVisible(detailEditors[index]);
    }

    if (mou)
    {
        countLabel.setText(utf8("分区"), juce::dontSendNotification);
        countLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(countLabel);
        for (int index = 0; index < 3; ++index)
        {
            const auto count = index + 2;
            auto& button = countButtons[static_cast<std::size_t>(index)];
            button.setButtonText(juce::String(count) + utf8(" 段分"));
            button.setTooltip(utf8("把所选条目改成 ") + juce::String(count)
                + utf8(" 个区，立即写入谋•OTO；保留仍在的音素字母，"
                       "新增的先当元音。"));
            button.setColour(juce::TextButton::buttonOnColourId, Palette::accent);
            button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            button.setClickingTogglesState(true);
            button.setRadioGroupId(0x6d6f76);
            button.setConnectedEdges(
                (index > 0 ? juce::Button::ConnectedOnLeft : 0)
                | (index < 2 ? juce::Button::ConnectedOnRight : 0));
            button.onClick = [this, count] { setSelectedRegionCount(count); };
            button.setEnabled(false);
            addAndMakeVisible(button);
        }
    }

    statusLabel.setColour(juce::Label::textColourId, Palette::textMuted);
    addAndMakeVisible(statusLabel);
    openEditorButton.setButtonText(utf8("打开编辑器"));
    openEditorButton.setTooltip(utf8("用波形和可拖动时序标记编辑所选 oto.ini 条目"));
    openEditorButton.onClick = [this]
    {
        juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
        juce::MessageManager::callAsync([safe]
        {
            if (safe != nullptr) safe->openSelectedEditor();
        });
    };
    openEditorButton.setEnabled(false);
    addAndMakeVisible(openEditorButton);
    duplicateButton.setButtonText(utf8("复制 OTO"));
    duplicateButton.setTooltip(utf8("复制所选 OTO 参数并填写新别名；不会复制音频文件"));
    duplicateButton.onClick = [this] { duplicateSelectedEntry(); };
    duplicateButton.setEnabled(false);
    addAndMakeVisible(duplicateButton);
    if (jie)
    {
        createJieButton.setButtonText(utf8("生成界•OTO"));
        createJieButton.setTooltip(utf8("按 oto.ini 复制出一份界•OTO（oto4.ini）；"
                                        "已有的条目不会被覆盖"));
        createJieButton.onClick = [this]
        {
            juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
            juce::MessageManager::callAsync([safe]
            {
                if (safe != nullptr) safe->createJieOto();
            });
        };
        addAndMakeVisible(createJieButton);
    }
    if (mou)
    {
        createMouButton.setButtonText(utf8("生成谋•OTO"));
        createMouButton.setTooltip(utf8("按界•OTO 复制出一份谋•OTO（otomou.ini），"
                                        "每条先标成 CVVV；已有的条目不会被覆盖"));
        createMouButton.onClick = [this]
        {
            juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
            juce::MessageManager::callAsync([safe]
            {
                if (safe != nullptr) safe->createMouOto();
            });
        };
        addAndMakeVisible(createMouButton);
    }
    reloadButton.setButtonText(utf8("刷新列表"));
    reloadButton.setTooltip(utf8("重新扫描音源库文件夹："
        "重读全部 oto.ini，并让渲染丢弃已缓存的解析结果"));
    reloadButton.onClick = [this] { refreshFromDisk(); };
    addAndMakeVisible(reloadButton);

    setSize(1040, 560);
    reload();
}

VoicebankSettingsComponent::~VoicebankSettingsComponent()
{
    table.setModel(nullptr);
}

void VoicebankSettingsComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::panel);
}

void VoicebankSettingsComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto top = area.removeFromTop(30);
    rootLabel.setBounds(top.removeFromLeft(std::max(260, top.getWidth() - 360)));
    searchLabel.setBounds(top.removeFromLeft(52));
    searchEditor.setBounds(top.reduced(0, 2));
    area.removeFromTop(8);
    auto bottom = area.removeFromBottom(32);
    reloadButton.setBounds(bottom.removeFromRight(96).reduced(0, 2));
    bottom.removeFromRight(8);
    openEditorButton.setBounds(bottom.removeFromRight(112).reduced(0, 2));
    bottom.removeFromRight(8);
    duplicateButton.setBounds(bottom.removeFromRight(104).reduced(0, 2));
    if (jie)
    {
        bottom.removeFromRight(8);
        createJieButton.setBounds(bottom.removeFromRight(118).reduced(0, 2));
    }
    if (mou)
    {
        bottom.removeFromRight(8);
        createMouButton.setBounds(bottom.removeFromRight(118).reduced(0, 2));
    }
    statusLabel.setBounds(bottom);
    area.removeFromBottom(8);

    auto details = area.removeFromRight(310);
    area.removeFromRight(10);
    table.setBounds(area);
    detailsTitle.setBounds(details.removeFromTop(28));
    details.removeFromTop(4);
    for (std::size_t index = 0; index < visibleDetailCount(); ++index)
    {
        auto row = details.removeFromTop(index == 7 ? 56 : 35);
        detailLabels[index].setBounds(row.removeFromLeft(74));
        detailEditors[index].setBounds(row.reduced(0, 3));
    }
    if (mou)
    {
        // Directly under the Jie oto row, so the boundaries and the number of
        // regions they divide read as one thing.
        auto row = details.removeFromTop(35);
        countLabel.setBounds(row.removeFromLeft(74));
        row = row.reduced(0, 3);
        const auto cell = row.getWidth() / 3;
        for (int index = 0; index < 3; ++index)
            countButtons[static_cast<std::size_t>(index)].setBounds(
                index < 2 ? row.removeFromLeft(cell) : row);
    }
}

int VoicebankSettingsComponent::getNumRows()
{
    return static_cast<int>(filteredRows.size());
}

void VoicebankSettingsComponent::paintRowBackground(juce::Graphics& g, int rowNumber,
                                                     int width, int height,
                                                     bool rowIsSelected)
{
    if (rowIsSelected) g.fillAll(Palette::accent.withAlpha(0.72f));
    else if ((rowNumber & 1) != 0) g.fillAll(Palette::panelRaised.withAlpha(0.45f));
    g.setColour(Palette::border.withAlpha(0.45f));
    g.drawHorizontalLine(height - 1, 0.0f, static_cast<float>(width));
}

void VoicebankSettingsComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId,
                                            int width, int height, bool rowIsSelected)
{
    if (!juce::isPositiveAndBelow(rowNumber, static_cast<int>(filteredRows.size()))) return;
    const auto& entry = entries[static_cast<std::size_t>(filteredRows[static_cast<std::size_t>(rowNumber)])];
    juce::String text;
    switch (columnId)
    {
        case 1: text = entry.sourceName; break;
        case 2: text = entry.alias; break;
        case 3: text = formatMilliseconds(entry.offsetMs); break;
        case 4: text = formatMilliseconds(entry.consonantMs); break;
        case 5: text = formatMilliseconds(entry.cutoffMs); break;
        case 6: text = formatMilliseconds(entry.preutteranceMs); break;
        case 7: text = formatMilliseconds(entry.overlapMs); break;
        case 8: text = jieSummary(entry); break;
        default: break;
    }
    g.setColour(rowIsSelected ? juce::Colours::white : Palette::text);
    g.setFont(13.0f);
    g.drawText(text, 6, 0, width - 10, height, columnId <= 2
        ? juce::Justification::centredLeft : juce::Justification::centredRight, true);
    g.setColour(Palette::border.withAlpha(0.45f));
    g.drawVerticalLine(width - 1, 0.0f, static_cast<float>(height));
}

void VoicebankSettingsComponent::selectedRowsChanged(int lastRowSelected)
{
    refreshDetails(lastRowSelected);
}

void VoicebankSettingsComponent::cellDoubleClicked(int rowNumber, int,
                                                    const juce::MouseEvent&)
{
    if (juce::isPositiveAndBelow(rowNumber, static_cast<int>(filteredRows.size())))
    {
        table.selectRow(rowNumber);
        juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
        juce::MessageManager::callAsync([safe]
        {
            if (safe != nullptr) safe->openSelectedEditor();
        });
    }
}

void VoicebankSettingsComponent::refreshFromDisk()
{
    // Reloading the table alone is not enough: the renderer keeps its own
    // parse of the voicebank, so without this the list would show a sample
    // that playback still knows nothing about.
    backend::UtauRenderer::invalidateVoicebankCache();
    reload();
}

void VoicebankSettingsComponent::reload()
{
    // In 谋 mode the panel has to read otomou.ini too, or every entry comes
    // back with no annotation and the region buttons have nothing to show.
    entries = SampleSettings::loadVoicebankOto(root, warnings, jie, mou);

    // A sample dropped into the folder without an oto row cannot be listed as
    // an entry -- there is nothing to edit or save.  Counting it is what turns
    // "the refresh did nothing" into "this file still needs an oto row".
    unregisteredAudio.clear();
    if (root.isDirectory())
    {
        juce::Array<juce::File> audioFiles;
        root.findChildFiles(audioFiles, juce::File::findFiles, true,
                            "*.wav;*.flac;*.aif;*.aiff");
        juce::StringArray referenced;
        for (const auto& entry : entries)
            referenced.add(entry.audioFile.getFullPathName().toLowerCase());
        for (const auto& file : audioFiles)
            if (!referenced.contains(file.getFullPathName().toLowerCase()))
                unregisteredAudio.add(file.getRelativePathFrom(root));
        unregisteredAudio.sortNatural();
    }
    applyFilter();
}

void VoicebankSettingsComponent::applyFilter()
{
    const auto filter = searchEditor.getText().trim();
    filteredRows.clear();
    for (std::size_t index = 0; index < entries.size(); ++index)
    {
        const auto& entry = entries[index];
        const auto searchable = entry.sourceName + "\n" + entry.alias + "\n"
            + entry.otoFile.getRelativePathFrom(root);
        if (filter.isEmpty() || searchable.containsIgnoreCase(filter))
            filteredRows.push_back(static_cast<int>(index));
    }
    table.updateContent();
    table.repaint();
    const auto warningText = warnings.isEmpty()
        ? juce::String{} : utf8("；警告 ") + juce::String(warnings.size());
    const auto strayText = unregisteredAudio.isEmpty() ? juce::String{}
        : utf8("；") + juce::String(unregisteredAudio.size()) + utf8(" 个音频尚未写入 oto");
    statusLabel.setText(utf8("共 ") + juce::String(entries.size()) + utf8(" 条，当前显示 ")
        + juce::String(filteredRows.size()) + warningText + strayText,
        juce::dontSendNotification);
    auto tooltip = warnings;
    if (!unregisteredAudio.isEmpty())
    {
        tooltip.add(utf8("尚未写入 oto 的音频："));
        tooltip.addArray(unregisteredAudio);
    }
    statusLabel.setTooltip(tooltip.joinIntoString("\n"));
    if (filteredRows.empty())
    {
        refreshDetails(-1);
        pendingAlias.clear();
        return;
    }
    auto row = 0;
    if (pendingAlias.isNotEmpty())
    {
        const auto target = SampleSettings::findEntryForAlias(entries, pendingAlias);
        for (std::size_t index = 0; index < filteredRows.size(); ++index)
            if (filteredRows[index] == target) { row = static_cast<int>(index); break; }
        pendingAlias.clear();
    }
    table.selectRow(row);
    table.scrollToEnsureRowIsOnscreen(row);
}

void VoicebankSettingsComponent::refreshDetails(int filteredRow)
{
    std::array<juce::String, 9> values;
    const auto valid = juce::isPositiveAndBelow(filteredRow,
                                                static_cast<int>(filteredRows.size()));
    openEditorButton.setEnabled(valid);
    duplicateButton.setEnabled(valid);
    if (valid)
    {
        const auto& entry = entries[static_cast<std::size_t>(
            filteredRows[static_cast<std::size_t>(filteredRow)])];
        values = { entry.sourceName, entry.alias,
            formatMilliseconds(entry.offsetMs) + " ms",
            formatMilliseconds(entry.consonantMs) + " ms",
            formatMilliseconds(entry.cutoffMs) + " ms",
            formatMilliseconds(entry.preutteranceMs) + " ms",
            formatMilliseconds(entry.overlapMs) + " ms",
            entry.otoFile.getRelativePathFrom(root),
            jieSummary(entry) };
    }
    for (std::size_t index = 0; index < visibleDetailCount(); ++index)
    {
        detailEditors[index].setText(values[index], false);
        detailEditors[index].setTooltip(values[index]);
    }
    syncRegionButtons();
}

juce::String VoicebankSettingsComponent::diagnosticClasses() const
{
    const auto* entry = selectedEntry();
    return entry != nullptr ? entry->mouClasses : juce::String();
}

int VoicebankSettingsComponent::diagnosticToggledCount() const
{
    for (int index = 0; index < 3; ++index)
        if (countButtons[static_cast<std::size_t>(index)].getToggleState())
            return index + 2;
    return 0;
}

void VoicebankSettingsComponent::syncRegionButtons()
{
    if (!mou) return;
    const auto* entry = selectedEntry();
    // Without an annotation an entry has the four regions an oto4 row has
    // always had, so that is what the buttons show.
    const auto count = entry != nullptr
        ? SampleSettings::mouRegionCount(entry->mouClasses) : 0;
    for (int index = 0; index < 3; ++index)
    {
        auto& button = countButtons[static_cast<std::size_t>(index)];
        button.setEnabled(entry != nullptr);
        button.setToggleState(index + 2 == count, juce::dontSendNotification);
    }
}

const VoicebankOtoEntry* VoicebankSettingsComponent::selectedEntry() const
{
    const auto selectedRow = table.getSelectedRow();
    if (!juce::isPositiveAndBelow(selectedRow, static_cast<int>(filteredRows.size())))
        return nullptr;
    const auto entryIndex = filteredRows[static_cast<std::size_t>(selectedRow)];
    if (!juce::isPositiveAndBelow(entryIndex, static_cast<int>(entries.size())))
        return nullptr;
    return &entries[static_cast<std::size_t>(entryIndex)];
}

void VoicebankSettingsComponent::setSelectedRegionCount(int count)
{
    const auto* selected = selectedEntry();
    if (!mou || selected == nullptr) return;
    count = juce::jlimit(2, 4, count);
    if (SampleSettings::mouRegionCount(selected->mouClasses) == count)
    {
        syncRegionButtons();
        return;
    }
    const auto original = *selected;
    auto edited = original;
    edited.mouClasses = SampleSettings::mouClassesForCount(original.mouClasses, count);
    juce::String error;
    if (!SampleSettings::updateMouOtoEntry(original, edited, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            utf8("无法保存谋•OTO（otomou.ini）"), error);
        syncRegionButtons();
        return;
    }
    backend::UtauRenderer::invalidateVoicebankCache();
    // The row order does not change, so the same row is still this entry.
    // Selecting it again may not fire a change, so the details are refreshed
    // by hand -- otherwise the buttons keep showing the count before the click.
    const auto row = table.getSelectedRow();
    reload();
    table.selectRow(row);
    refreshDetails(row);
    statusLabel.setText(original.sourceName + utf8(" 分区：") + edited.mouClasses,
                        juce::dontSendNotification);
}

void VoicebankSettingsComponent::openSelectedEditor()
{
    const auto selectedRow = table.getSelectedRow();
    if (!juce::isPositiveAndBelow(selectedRow, static_cast<int>(filteredRows.size()))) return;
    const auto entryIndex = filteredRows[static_cast<std::size_t>(selectedRow)];
    if (!juce::isPositiveAndBelow(entryIndex, static_cast<int>(entries.size()))) return;
    const auto entry = entries[static_cast<std::size_t>(entryIndex)];

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = (mou ? utf8("谋•OTO 分区编辑器 — ")
                          : jie ? utf8("界•OTO 四区编辑器 — ")
                                : utf8("oto 时序编辑器 — "))
        + entry.sourceName;
    options.dialogBackgroundColour = Palette::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
    options.content.setOwned(new OtoWaveformEditorComponent(entry, jie, mou, [safe]
    {
        if (safe != nullptr) safe->reload();
    }));
    if (auto* window = options.launchAsync())
        window->setResizeLimits(720, 400, 1800, 1100);
}

void VoicebankSettingsComponent::duplicateSelectedEntry()
{
    const auto selectedRow = table.getSelectedRow();
    if (!juce::isPositiveAndBelow(selectedRow, static_cast<int>(filteredRows.size()))) return;
    const auto entryIndex = filteredRows[static_cast<std::size_t>(selectedRow)];
    if (!juce::isPositiveAndBelow(entryIndex, static_cast<int>(entries.size()))) return;
    const auto source = entries[static_cast<std::size_t>(entryIndex)];

    const auto aliasBase = source.alias.trim().isNotEmpty() ? source.alias.trim()
                                                             : source.sourceName;
    auto suggested = aliasBase + "_copy";
    const auto aliasExists = [this](const juce::String& alias)
    {
        return std::any_of(entries.begin(), entries.end(), [&](const auto& entry)
        {
            return entry.alias.equalsIgnoreCase(alias);
        });
    };
    for (auto suffix = 2; aliasExists(suggested); ++suffix)
        suggested = aliasBase + "_copy" + juce::String(suffix);

    auto* dialog = new juce::AlertWindow(
        utf8("复制 OTO 条目"),
        utf8("只复制 OTO 参数并继续引用原音频文件。请输入新别名："),
        juce::MessageBoxIconType::NoIcon);
    dialog->addTextEditor("alias", suggested, utf8("新别名："));
    if (auto* editor = dialog->getTextEditor("alias"))
        editor->setSelectAllWhenFocused(true);
    dialog->addButton(utf8("复制"), 1);
    dialog->addButton(utf8("取消"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<VoicebankSettingsComponent> safe(this);
    dialog->enterModalState(true,
        juce::ModalCallbackFunction::create(
            [safe, dialog, source](int result)
            {
                if (safe != nullptr && result == 1)
                {
                    const auto alias = dialog->getTextEditorContents("alias").trim();
                    juce::String error;
                    if (!SampleSettings::duplicateVoicebankOtoEntry(
                            source, alias, safe->jie, error))
                    {
                        juce::AlertWindow::showMessageBoxAsync(
                            juce::MessageBoxIconType::WarningIcon,
                            utf8("无法复制 OTO"), error);
                    }
                    else
                    {
                        backend::UtauRenderer::invalidateVoicebankCache();
                        safe->pendingAlias = alias;   // 跳到刚复制出来的条目
                        safe->reload();
                        safe->statusLabel.setText(
                            utf8("已复制 OTO，新别名：") + alias,
                            juce::dontSendNotification);
                    }
                }
                delete dialog;
            }), false);
}

juce::String VoicebankSettingsComponent::formatMilliseconds(double value)
{
    auto text = juce::String(value, 3);
    while (text.containsChar('.') && text.endsWithChar('0')) text = text.dropLastCharacters(1);
    if (text.endsWithChar('.')) text = text.dropLastCharacters(1);
    return text;
}

juce::String VoicebankSettingsComponent::jieSummary(const VoicebankOtoEntry& entry)
{
    if (!entry.hasJieOto) return utf8("未拓展");
    return formatMilliseconds(entry.jieOnsetMs) + " / "
        + formatMilliseconds(entry.jieGlideMs) + " / "
        + formatMilliseconds(entry.jieNucleusMs);
}

void VoicebankSettingsComponent::createMouOto()
{
    auto written = 0;
    auto kept = 0;
    auto merged = 0;
    juce::String error;
    if (!SampleSettings::createMouOto(root, written, kept, merged, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            utf8("无法生成谋•OTO"), error);
        return;
    }
    backend::UtauRenderer::invalidateVoicebankCache();
    reload();
    statusLabel.setText(utf8("谋•OTO：新建 ") + juce::String(written)
        + utf8(" 条，保留已有 ") + juce::String(kept) + utf8(" 条")
        + (merged > 0 ? utf8("，合并重复 ") + juce::String(merged)
                        + utf8(" 条") : juce::String()),
        juce::dontSendNotification);
}

void VoicebankSettingsComponent::createJieOto()
{
    auto written = 0;
    auto kept = 0;
    juce::String error;
    if (!SampleSettings::createJieOto(root, written, kept, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            utf8("无法生成界•OTO"), error);
        return;
    }
    backend::UtauRenderer::invalidateVoicebankCache();
    reload();
    statusLabel.setText(utf8("界•OTO：新建 ") + juce::String(written)
        + utf8(" 条，保留已有 ") + juce::String(kept) + utf8(" 条"),
        juce::dontSendNotification);
}
}
