#pragma once

#include <juce_core/juce_core.h>

namespace hachi
{
// The Mandarin pinyin of a Chinese character, as a UTAU voicebank spells its
// aliases: lower case, no tone, u umlaut written v (lv, nve).  The first --
// most common -- reading; polyphones are not told apart.  nullptr for a
// character that is not Chinese or not in the table.
[[nodiscard]] const char* pinyinFor(juce::juce_wchar character);

// A lyric with every Chinese character in it replaced by its pinyin, and
// everything else -- the "- " of a VCV alias, a trailing " R", latin, digits,
// punctuation -- left exactly as it was.  Characters that sit next to each
// other run together ("你好" -> "nihao").
[[nodiscard]] juce::String lyricInPinyin(const juce::String& lyric);
}
