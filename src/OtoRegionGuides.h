#pragma once
#include "Theme.h"
#include <algorithm>
#include <array>
#include <vector>

namespace hachi
{
// Display-only, relative to the nominal note start (including its lead-in).
struct OtoRegionGuide
{
    double startSeconds = 0.0, endSeconds = 0.0;
    juce::String label;
};

template<class TimeToX>
void paintOtoRegionGuides(juce::Graphics& g, juce::Rectangle<float> plot,
                         const std::vector<OtoRegionGuide>& regions, TimeToX timeToX)
{
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(plot.toNearestInt());
    static const std::array<juce::Colour, 4> colours {
        juce::Colour(0xffff7043), juce::Colour(0xffffca28),
        juce::Colour(0xff4fc3f7), juce::Colour(0xff81c784)
    };
    g.setFont(11.0f);
    for (size_t i = 0; i < regions.size(); ++i)
    {
        const auto& region = regions[i];
        const auto left = timeToX(region.startSeconds), right = timeToX(region.endSeconds);
        const auto colour = colours[std::min(i, colours.size() - 1)];
        const auto band = juce::Rectangle<float>(left, plot.getY(), std::max(0.0f, right - left), plot.getHeight());
        g.setColour(colour.withAlpha(0.10f)); g.fillRect(band);
        g.setColour(colour.withAlpha(0.72f));
        g.drawLine(left, plot.getY(), left, plot.getBottom(), 1.0f);
        if (i + 1 == regions.size()) g.drawLine(right, plot.getY(), right, plot.getBottom(), 1.0f);
        const auto visible = band.getIntersection(plot);
        if (visible.getWidth() >= 32.0f)
        {
            g.setColour(colour.withAlpha(0.95f));
            g.drawFittedText(region.label, visible.withHeight(18).reduced(3, 0).toNearestInt(),
                            juce::Justification::centred, 1, 0.8f);
        }
    }
}
}
