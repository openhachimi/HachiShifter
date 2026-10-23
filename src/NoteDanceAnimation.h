#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <vector>
#include <cmath>
#include "NoteDanceData.h"

namespace hachi
{
// GIF frames are embedded as lossless RGBA atlases; no runtime files or decoder needed.
class NoteDanceAnimation
{
public:
    void setEnabled(bool value)
    {
        if (enabled == value) return;
        enabled = value;
        if (enabled)
        {
            if (!loaded)
            {
                #include "../resources/note-dance/frames.inc"
                loaded = true;
            }
            started = juce::Time::getMillisecondCounterHiRes();
            elapsed = 0.0;
        }
    }
    bool isEnabled() const { return enabled; }
    void advance() { elapsed = juce::Time::getMillisecondCounterHiRes() - started; }
    void draw(juce::Graphics& g, std::size_t index, juce::Rectangle<float> note, float height) const
    {
        if (!enabled) return;
        const auto& a = atlases[index % atlases.size()];
        const auto width = height * static_cast<float>(a.width) / static_cast<float>(a.height);
        const juce::Rectangle<float> dest(note.getCentreX() - width * 0.5f,
                                          note.getY() - height, width, height);
        if (!g.getClipBounds().toFloat().intersects(dest)) return;
        const auto frame = frameFor(index, elapsed);
        const auto image = a.image.getClippedImage({ frame * a.width, 0, a.width, a.height });
        g.setOpacity(1.0f);
        g.drawImage(image, dest);
    }
    int frameFor(std::size_t index, double ms) const
    {
        const auto& ends = atlases[index % atlases.size()].ends;
        if (ends.empty()) return 0;
        const auto t = std::fmod(std::max(0.0, ms), static_cast<double>(ends.back()));
        for (std::size_t i = 0; i < ends.size(); ++i)
            if (t < ends[i]) return static_cast<int>(i);
        return 0;
    }
private:
    struct Atlas { juce::Image image; int width = 1, height = 1; std::vector<int> ends; };
    std::array<Atlas, 4> atlases;
    bool enabled = false, loaded = false;
    double started = 0.0, elapsed = 0.0;
    void load(std::size_t index, const char* bytes, int size, int width, int height,
              std::initializer_list<int> ends)
    {
        atlases[index] = { juce::ImageFileFormat::loadFrom(bytes, static_cast<size_t>(size)),
                           width, height, ends };
    }
};
}
