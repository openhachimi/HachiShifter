#pragma once
#include <juce_core/juce_core.h>

namespace hachi
{
// Language namespaces are model identifiers, not the spoken phoneme. Keep the
// full token for synthesis, selection details and tooltips.
inline juce::String diffSingerPhonemeLabel(const juce::String& token)
{
    const auto slash = token.indexOfChar('/');
    return slash > 0 && slash + 1 < token.length() ? token.substring(slash + 1) : token;
}
}
