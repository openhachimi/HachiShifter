#pragma once
#include "../I18n.h"
#include <iostream>
namespace hachi
{
// All expected sentinel values below exist ONLY in the test's on-disk TSV.
// This is deliberately not an embedded language fallback or a shipped table.
inline bool runTranslationFileSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("translations-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto file=root.getChildFile("translations.tsv");
    const auto loaded=I18n::loadTranslations();check("external_shipped_catalog_loads",loaded.loaded&&loaded.rows>1000);
    const auto header=juce::String("key\tzh-CN\tzh-TW\tja-JP\tko-KR\ten-US\tnote\taliases\n");
    const auto content=header+"editbar.more\tFILE-ZH\tFILE-TW\tFILE-JA\tFILE-KO\tFILE-EN\t\t[\"More...\",\"更多…\"]\n"
        "material.uses\tZH {count}\tTW {count}\tJA {count}\tKO {count}\tEN {count}\t\t[\"FILE_COUNT {count}\"]\n"
        "Recover current prediction\t预测-ZH\t預測-TW\t予測-JA\t예측-KO\tPrediction-EN\t\t[\"恢复当前预测\"]\n"
        "fallback\t\t\t\t\tONLY-FILE-EN\t\t[]\n";
    file.replaceWithText(content);const auto report=I18n::loadTranslations(file);I18n strings;
    check("all_file_rows_are_retained_even_without_builtin_overrides",report.loaded&&report.rows==4);
    const std::array<juce::String,5> expected{"FILE-ZH","FILE-TW","FILE-JA","FILE-KO","FILE-EN"};bool languages=true;
    for(int i=0;i<5;++i){strings.setLanguage(I18n::Language(i));languages&=strings.text("editbar.more")==expected[std::size_t(i)]&&I18n::uiText("More...")==expected[std::size_t(i)]&&I18n::uiText("更多…")==expected[std::size_t(i)];}
    check("every_language_and_literal_alias_comes_from_file",languages);
    strings.setLanguage(I18n::Language::jaJP);
    I18n child;check("new_UI_components_inherit_selected_language_not_OS_locale",child.getLanguage()==I18n::Language::jaJP&&I18n::uiText("More...")=="FILE-JA");
    check("dynamic_diagnostic_template_and_value_both_use_file_catalog",I18n::message("FILE_COUNT 7")=="JA 7");
    check("pair_helper_cannot_fall_back_to_its_chinese_argument",I18n::pairText("THIS_MUST_NEVER_BE_DISPLAYED","More...",I18n::Language::jaJP)=="FILE-JA");
    check("missing_key_does_not_resurrect_the_old_embedded_table",strings.text("track.compose")=="[track.compose]");
    check("empty_cell_falls_back_only_to_english_in_same_file",strings.text("fallback")=="ONLY-FILE-EN");
    file.replaceWithText(content.replace("FILE-JA","EDITED-IN-FILE"));I18n::loadTranslations(file);
    check("reloading_file_changes_existing_I18n_objects",strings.text("editbar.more")=="EDITED-IN-FILE"&&I18n::uiText("More...")=="EDITED-IN-FILE");
    const auto copy=root.getChildFile("copy.tsv");check("export_copies_file_catalog_and_preserves_aliases",I18n::exportTranslations(copy,error)&&copy.loadFileAsString().contains("EDITED-IN-FILE")&&copy.loadFileAsString().contains("aliases"));
    file.replaceWithText(content.replace("JA {count}","bad-placeholder"));const auto badCell=I18n::loadTranslations(file);
    check("invalid_placeholder_cell_is_rejected_using_file_reference",badCell.loaded&&badCell.rejected==1&&strings.text("material.uses")=="EN {count}");
    file.replaceWithText("not a translation TSV\n");const auto invalid=file.loadFileAsString();const auto broken=I18n::loadTranslations(file);
    check("bad_file_retains_last_good_file_catalog",!broken.loaded&&!broken.problems.isEmpty()&&strings.text("material.uses")=="EN {count}");
    check("export_never_rewrites_unknown_format_file",!I18n::exportTranslations(file,error)&&file.loadFileAsString()==invalid);
    const auto missing=root.getChildFile("missing.tsv");const auto absent=I18n::loadTranslations(missing);
    check("missing_file_reports_error_without_compiled_fallback",!absent.loaded&&!absent.problems.isEmpty()&&strings.text("editbar.more")=="FILE-JA");
    file.replaceWithText(header+"same\t\t\tFIRST\t\tEN\t\t[]\nsame\t\t\tSECOND\t\tEN\t\t[]\n");const auto duplicate=I18n::loadTranslations(file);
    check("duplicate_key_first_wins_and_reports",duplicate.loaded&&duplicate.rejected==1&&strings.text("same")=="FIRST");
    const auto restored=I18n::loadTranslations();check("shipped_file_catalog_restored_for_other_UI_tests",restored.loaded&&restored.rows>1000);
    root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"translation_fixture="<<root.getFullPathName()<<std::endl;return ok;
}
inline bool runTranslationNoFileSmoke(const juce::File& output)
{
    output.createDirectory();const auto report=I18n::loadTranslations(output.getChildFile("does-not-exist.tsv"));
    I18n strings;strings.setLanguage(I18n::Language::zhCN);juce::String error;
    const bool noFallback=!report.loaded&&!report.problems.isEmpty()&&strings.text("track.compose")=="[track.compose]"
        &&I18n::uiText("More...")=="[More...]"&&I18n::pairText("THIS_COMPILED_CHINESE_MUST_NOT_APPEAR","Legacy English",I18n::Language::zhCN)=="[Legacy English]"
        &&!I18n::exportTranslations(output.getChildFile("must-not-generate.tsv"),error);
    output.getChildFile("verification.txt").replaceWithText("fresh_process_without_language_file_has_no_compiled_translations="+juce::String(noFallback?"PASS":"FAIL")+"\n"+report.problems.joinIntoString("\n"));
    return noFallback;
}
}
