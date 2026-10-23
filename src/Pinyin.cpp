#include "Pinyin.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace hachi
{
namespace
{
struct PinyinEntry
{
    std::uint32_t codepoint;
    std::uint16_t syllable;
};

#include "PinyinTable.inc"
}

const char* pinyinFor(juce::juce_wchar character)
{
    const auto codepoint = static_cast<std::uint32_t>(character);
    const auto found = std::lower_bound(std::begin(pinyinEntries), std::end(pinyinEntries),
        codepoint, [](const PinyinEntry& entry, std::uint32_t value)
        {
            return entry.codepoint < value;
        });
    if (found == std::end(pinyinEntries) || found->codepoint != codepoint) return nullptr;
    return pinyinSyllables[found->syllable];
}

juce::String lyricInPinyin(const juce::String& lyric)
{
    juce::String converted;
    converted.preallocateBytes(lyric.getNumBytesAsUTF8() * 2);
    for (auto cursor = lyric.getCharPointer(); !cursor.isEmpty();)
    {
        const auto character = cursor.getAndAdvance();
        if (const auto* syllable = pinyinFor(character))
            converted << syllable;
        else
            converted << juce::String::charToString(character);
    }
    return converted;
}
}
