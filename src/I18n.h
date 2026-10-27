#pragma once

#include <juce_core/juce_core.h>

namespace hachi
{
class I18n
{
public:
    enum class Language { zhCN, zhTW, jaJP, koKR, enUS };

    I18n();
    void setLanguage(Language languageToUse);
    // Legacy UI literals only; never use for model data, IDs or user input.
    [[nodiscard]] static juce::String uiText(const char* sourceLiteral);
    [[nodiscard]] Language getLanguage() const { return language; }
    [[nodiscard]] juce::String text(const juce::String& key) const;
    [[nodiscard]] juce::String text(const char* utf8Key) const { return text(juce::String::fromUTF8(utf8Key)); }

    // External, hand-editable translations (bin/lang/translations.tsv).
    // Columns: key, zh-CN, zh-TW, ja-JP, ko-KR, en-US, note, aliases (JSON).
    // ALL display values come from this file. An empty cell may use the file's
    // English cell; missing keys are visibly marked [key], never translated by
    // a compiled table. Bad/missing files retain the last good FILE catalog.
    // Placeholders like {count} must match the file's English reference.
    struct LoadReport { bool loaded = false; int rows = 0, overrides = 0, rejected = 0; juce::StringArray problems; };
    static juce::File translationFile();
    static LoadReport loadTranslations(const juce::File& file = translationFile());
    // Writes the loaded FILE catalog plus extra marked rows; existing cells
    // win. No language values are generated from C++ literals or tables.
    static bool exportTranslations(const juce::File& file, juce::String& error,
                                   const juce::StringArray& extraLiterals = {});
    static int builtInRowCount(); // compatibility name: count of loaded FILE rows
    static juce::StringArray missingTranslationKeys();
    static void clearMissingTranslationKeys();
    // UI boundary for diagnostics: exact or one-parameter aliases from the
    // same file, preserving technical/user detail when there is no mapping.
    [[nodiscard]] static juce::String message(const juce::String& diagnostic);
    // Compatibility pair: English is a stable file alias; zh is ignored at
    // runtime (never a Chinese fallback). The file supplies every language.
    [[nodiscard]] static juce::String pairText(const char* zh, const char* en, Language);

private:
    Language language = Language::zhCN;
};
}

