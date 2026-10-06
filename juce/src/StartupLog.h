#pragma once
#include <juce_core/juce_core.h>

namespace hachi
{
inline void startupLog(const juce::String& stage)
{
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("HachiShifterNext");
    if (directory.createDirectory().failed()) return;
    static const auto session = juce::Uuid().toDashedString();
    const auto file = directory.getChildFile("startup-" + session + ".log");
    file.appendText(juce::Time::getCurrentTime().toISO8601(true) + " " + stage + "\n");
}
}
