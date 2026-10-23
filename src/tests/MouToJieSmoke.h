#pragma once
#include "../VoicebankSettingsComponent.h"
#include "../backend/LegacyTextCodec.h"
#include <iostream>

namespace hachi
{
inline bool mouToJieSmoke(const juce::File& folder)
{
    folder.createDirectory();
    bool ok = true; juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool pass)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name); row->setProperty("ok", pass);
        checks.add(juce::var(row)); ok &= pass; std::cout << name << '=' << pass << std::endl;
    };
    const auto write = [&](const juce::File& file, const juce::String& text)
    {
        backend::LegacyTextDocument doc; juce::String error;
        if (file.existsAsFile()) doc = *backend::LegacyTextCodec::read(file, error);
        else doc.codePage = 932;
        check("fixture_cp932_written", backend::LegacyTextCodec::write(file, text, doc, error));
    };
    const auto read = [&](const juce::File& file)
    {
        juce::String error; const auto doc = backend::LegacyTextCodec::read(file, error);
        return doc ? doc->text : juce::String{};
    };
    const auto hash = [](const juce::File& file) { return juce::SHA256(file).toHexString(); };
    const auto bank = folder.getChildFile("bank"); bank.createDirectory();
    const auto sub = bank.getChildFile("C5"); sub.createDirectory();
    const auto jp = juce::String::fromUTF8("あ.wav");
    const auto classic = "tone.wav=a,10,20,-500,30,-5\r\ntone.wav=aa,10,20,-500,30,-5\r\n"
        "tone.wav=second,200,20,-500,30,-5\r\nthree.wav=three,0,20,-500,30,-5\r\n"
        "missing.wav=missing,0,20,-500,30,-5\r\ninvalid.wav=invalid,0,20,-500,30,-5\r\n"
        "unordered.wav=unordered,0,20,-500,30,-5\r\nduplicate.wav=duplicate,0,20,-500,30,-5\r\n";
    write(bank.getChildFile("oto.ini"), juce::String(classic) + jp + "=jp,7,20,-500,30,-5\r\n");
    write(bank.getChildFile("oto.jie.ini"), read(bank.getChildFile("oto.ini")).replace(",10,20,", ",20,20,"));
    const auto mouText = juce::String("; source\r\ntone.wav=CVVC,20,35,100,260,0.5,2,3,4\r\n"
        "tone.wav=CV,200,100,200,300\r\nthree.wav=CVV,0,10,20,30\r\n"
        "invalid.wav=CVVV,0,broken,20,30\r\nunordered.wav=CVVV,0,30,20,10\r\n"
        "duplicate.wav=CVVV,0,12,24,48\r\nduplicate.wav=CV,0,60,70,80\r\n"
        "orphan.wav=CVVV,0,1,2,3\r\n") + jp + "=SVVC,7,44,88,222\r\n";
    write(bank.getChildFile("otomou.ini"), mouText);
    const auto preserved = juce::String("tone.wav=200,1,2,3\nthree.wav=0,2,3,4\r\n"
        "missing.wav=0,3,4,5\r\ninvalid.wav=0,4,5,6\r\nunordered.wav=0,5,6,7\r\n"
        "duplicate.wav=0,7,8,9\r\norphan-target.wav=0,9,10,11");
    // Non-ASCII content makes the original CP932 encoding observable; an
    // ASCII-only file is also valid UTF-8 and has no detectable legacy codec.
    const auto oldJie = juce::String::fromUTF8("; keep comment あ\r\ntone.wav=20,20,20,450,9,9,9,9\r\n") + preserved;
    write(bank.getChildFile("oto4.ini"), oldJie);
    write(sub.getChildFile("oto.ini"), "tone.wav=a,20,20,-500,30,-5\r\n");
    write(sub.getChildFile("otomou.ini"), "tone.wav=VVCC,20,11,22,333\r\n");
    const auto originalHash = hash(bank.getChildFile("oto.ini"));
    const auto timingHash = hash(bank.getChildFile("oto.jie.ini"));
    const auto mouHash = hash(bank.getChildFile("otomou.ini"));
    const auto revision = SampleSettings::voicebankFilesRevision();
    SampleSettings::JieFromMouResult result; juce::String error;
    check("conversion_succeeds", SampleSettings::createJieOtoFromMou(bank, result, error));
    check("all_four_region_rows_converted_once", result.written == 3 && result.skipped == 6);
    const auto converted = read(bank.getChildFile("oto4.ini"));
    check("three_boundaries_and_policy_transferred", converted.contains("tone.wav=20,35,100,260,0.5,2,3,4"));
    check("nonmatching_rows_and_line_endings_preserved", converted.contains(preserved));
    check("all_four_region_class_patterns_accepted", converted.contains(jp + "=7,44,88,222"));
    check("no_unmatched_source_inserted", !converted.contains("\norphan.wav="));
    check("subdirectories_keep_their_own_divisions", read(sub.getChildFile("oto4.ini")).contains("tone.wav=20,11,22,333"));
    check("original_oto_and_timing_unchanged", originalHash == hash(bank.getChildFile("oto.ini"))
        && timingHash == hash(bank.getChildFile("oto.jie.ini")));
    check("mou_source_unchanged", mouHash == hash(bank.getChildFile("otomou.ini")));
    check("render_cache_revision_updated", SampleSettings::voicebankFilesRevision() > revision);
    const auto backups = bank.findChildFiles(juce::File::findFiles, false, "oto4.before-mou*.bak");
    check("previous_jie_divisions_backed_up", backups.size() == 1 && read(backups[0]) == oldJie);
    const auto encoded = backend::LegacyTextCodec::read(bank.getChildFile("oto4.ini"), error);
    check("japanese_encoding_preserved", encoded && encoded->codePage == 932);
    const auto firstHash = hash(bank.getChildFile("oto4.ini"));
    check("repeat_succeeds_without_rewriting", SampleSettings::createJieOtoFromMou(bank, result, error)
        && result.written == 0 && result.unchanged == 3 && hash(bank.getChildFile("oto4.ini")) == firstHash);
    check("repeat_does_not_make_another_backup", bank.findChildFiles(juce::File::findFiles, false, "oto4.before-mou*.bak").size() == 1);
    juce::StringArray warnings;
    const auto rows = SampleSettings::loadVoicebankOto(bank, warnings, true);
    const auto alias = std::find_if(rows.begin(), rows.end(), [&](const auto& row)
        { return row.alias == "a" && row.otoFile.getParentDirectory() == bank; });
    check("jie_reader_sees_imported_four_divisions", alias != rows.end() && alias->hasJieOto
        && alias->jieOnsetMs == 35 && alias->jieGlideMs == 100 && alias->jieNucleusMs == 260);

    HachiLookAndFeel look;
    VoicebankSettingsComponent panel(bank, true, false, "jp");
    panel.setLookAndFeel(&look); panel.setSize(1040, 650);
    const auto selected = panel.diagnosticSelectedEntry();
    const auto selectedAlias = selected ? selected->alias : juce::String{};
    juce::TextButton *fromMou = nullptr, *fromOto = nullptr;
    for (auto* child : panel.getChildren()) if (auto* button = dynamic_cast<juce::TextButton*>(child))
    {
        if (button->getButtonText() == juce::String::fromUTF8("根据谋 OTO 生成")) fromMou = button;
        if (button->getButtonText() == juce::String::fromUTF8("根据 OTO 生成")) fromOto = button;
    }
    check("two_generation_buttons_visible_and_separate", fromMou && fromOto && fromMou->isVisible() && fromOto->isVisible()
        && !fromMou->getBounds().intersects(fromOto->getBounds()));
    write(bank.getChildFile("otomou.ini"), mouText.replace("44,88,222", "45,90,225"));
    panel.diagnosticCreateJieFromMou();
    check("button_imports_and_refreshes_selected_entry", panel.diagnosticSelectedEntry()
        && panel.diagnosticSelectedEntry()->alias == selectedAlias && panel.diagnosticSelectedEntry()->jieGlideMs == 90);
    {
        juce::PNGImageFormat png; auto stream = folder.getChildFile("jie-generation-buttons.png").createOutputStream();
        check("button_layout_snapshot", stream && png.writeImageToStream(panel.createComponentSnapshot(panel.getLocalBounds()), *stream));
    }
    panel.setLookAndFeel(nullptr);
    VoicebankSettingsComponent classicPanel(bank, false, false), mouPanel(bank, true, true);
    check("conversion_button_only_in_jie_mode", !classicPanel.diagnosticJieFromMouVisible() && !mouPanel.diagnosticJieFromMouVisible());
    const auto shortBank = folder.getChildFile("only-two-regions"); shortBank.createDirectory();
    write(shortBank.getChildFile("oto.ini"), "tone.wav=a,0,20,-500,30,0\r\n");
    write(shortBank.getChildFile("otomou.ini"), "tone.wav=CV,0,10,20,30\r\n");
    check("two_regions_with_three_stored_boundaries_are_not_four_regions",
        SampleSettings::createJieOtoFromMou(shortBank, result, error) && result.written == 0 && result.skipped == 1
        && !shortBank.getChildFile("oto4.ini").existsAsFile());
    const auto fresh = folder.getChildFile("from-oto"); fresh.createDirectory();
    write(fresh.getChildFile("oto.ini"), "tone.wav=a,0,20,-500,30,0\r\n");
    int written = 0, kept = 0;
    check("original_oto_generation_creates_independent_timing", SampleSettings::createJieOto(fresh, written, kept, error)
        && written == 1 && fresh.getChildFile("oto.jie.ini").existsAsFile() && fresh.getChildFile("oto4.ini").existsAsFile());
    const auto generated = read(fresh.getChildFile("oto4.ini"));
    check("original_oto_generation_remains_repeatable", SampleSettings::createJieOto(fresh, written, kept, error)
        && written == 0 && kept == 1 && generated == read(fresh.getChildFile("oto4.ini")));
    auto* report = new juce::DynamicObject(); report->setProperty("ok", ok); report->setProperty("checks", checks);
    folder.getChildFile("mou-to-jie-validation.json").replaceWithText(juce::JSON::toString(juce::var(report), true));
    return ok;
}
}
