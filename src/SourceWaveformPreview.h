#pragma once
#include "NativeSourceTimeMap.h"
#include "RenderedWaveformPeaks.h"
#include <juce_audio_utils/juce_audio_utils.h>

namespace hachi
{
// 32 source samples per cached peak, eight times finer than the old preview.
// JUCE loads these peaks off the UI thread; the source recording stays shared.
inline constexpr int nativeWaveformSamplesPerPeak = 32;

// Draw loudness around the written pitch rather than signed sample oscillation.
// Half-pixel, antialiased paths keep stretched onsets readable; every column
// still queries its complete source span so zooming out cannot skip a transient.
enum class NativeWaveformTint { warm, paleBlue };
template<typename PeakAt>
inline void drawNativePeakShape(juce::Graphics& g, double durationSeconds, PeakAt peakAt,
    juce::Rectangle<float> bounds, float opacity, bool selected, NativeWaveformTint tint,
    juce::Colour phase=juce::Colour(0xff7f69ca))
{
    if (durationSeconds <= 1.0e-9 || bounds.getWidth() < 1.0f
        || bounds.getHeight() <= 0.0f) return;
    const auto visible = bounds.getIntersection(g.getClipBounds().toFloat());
    if (visible.isEmpty()) return;
    const auto first = std::max(bounds.getX(), std::floor(visible.getX()) - 1.0f);
    const auto last = std::min(bounds.getRight(), std::ceil(visible.getRight()) + 1.0f);
    const auto targetAt = [&](float x)
    {
        return juce::jlimit(0.0, durationSeconds,
            (x - bounds.getX()) / bounds.getWidth() * durationSeconds);
    };
    const auto centre = bounds.getCentreY();
    std::vector<juce::Point<float>> outline;
    outline.reserve(static_cast<std::size_t>(std::max(0.0f, (last-first)*2.0f+4.0f)));
    auto peakHeight = 0.0f;
    const auto flush = [&]
    {
        if (outline.empty()) return;
        juce::Path body;
        body.startNewSubPath(outline.front().x, centre);
        for (const auto& point : outline) body.lineTo(point);
        body.lineTo(outline.back().x, centre);
        for (auto point=outline.rbegin(); point!=outline.rend(); ++point)
            body.lineTo(point->x, 2.0f*centre-point->y);
        body.closeSubPath();
        const auto half = std::max(1.0f, peakHeight);
        juce::ignoreUnused(tint);
        // A restrained, single-hue ink envelope belongs to the editor palette.
        // No red/orange/cream spectral bands or imitation heat-map glow.
        const auto ink=selected?phase.interpolatedWith(juce::Colours::white,.32f):phase;
        juce::ColourGradient gradient(ink.darker(.18f).withAlpha(opacity),0,centre-half,
                                      ink.darker(.18f).withAlpha(opacity),0,centre+half,false);
        gradient.addColour(.5,ink.interpolatedWith(juce::Colours::white,.30f).withAlpha(opacity));
        g.setGradientFill(gradient);g.fillPath(body);
        g.setColour(ink.interpolatedWith(juce::Colours::white,.60f).withAlpha(opacity));
        g.strokePath(body,juce::PathStrokeType(selected ? 1.3f : .8f,
            juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        outline.clear();peakHeight=0.0f;
    };
    for (auto x=first; x<last; x+=0.5f)
    {
        const auto amplitude=peakAt(targetAt(x),targetAt(std::min(x+0.5f,last)));
        if(amplitude<=1.0e-6f)
        {
            if(!outline.empty()) {outline.emplace_back(x,centre);flush();}
            continue;
        }
        const auto half=juce::jlimit(0.0f,1.0f,amplitude)*bounds.getHeight()*0.5f;
        if(outline.empty())outline.emplace_back(std::max(first,x-0.5f),centre);
        outline.emplace_back(x,centre-half);peakHeight=std::max(peakHeight,half);
    }
    flush();
}
inline void drawNativeSourceWaveform(juce::Graphics& g,juce::AudioThumbnail& thumbnail,
    const ClipData& audioClip,const std::vector<SourceTimePoint>& map,
    juce::Rectangle<float> bounds,float opacity=0.94f,bool selected=false,juce::Colour phase=juce::Colour(0xff7f69ca))
{
    if(thumbnail.getTotalLength()<=0.0)return;
    const auto peakAt=[&](double from,double to)
    {
        auto begin=audioClip.sourceOffsetSeconds+nativeSourceTimeAt(map,from);
        auto end=audioClip.sourceOffsetSeconds+nativeSourceTimeAt(map,to);
        float low=0.0f,high=0.0f;
        if(end>0.0&&begin<thumbnail.getTotalLength()&&end>begin)
            thumbnail.getApproximateMinMax(std::max(0.0,begin),std::min(thumbnail.getTotalLength(),end),0,low,high);
        const auto amplitude=std::max(std::abs(low),std::abs(high));
        return amplitude<=1.0f/128.0f ? 0.0f : amplitude;
    };
    drawNativePeakShape(g,audioClip.durationSeconds,peakAt,bounds,opacity,selected,NativeWaveformTint::warm,phase);
}
inline void drawNativeRenderedWaveform(juce::Graphics& g,const NativeRenderedPeaks& peaks,
    double durationSeconds,juce::Rectangle<float> bounds,float opacity=0.94f,bool selected=false,juce::Colour phase=juce::Colour(0xff7f69ca))
{
    const auto peakAt=[&](double from,double to)
    {
        if(peaks.sampleRate<=0.0||from>=peaks.sampleCount/peaks.sampleRate)return 0.0f;
        const auto first=std::max(0,static_cast<int>(std::floor(from*peaks.sampleRate/NativeRenderedPeaks::samplesPerBucket)));
        const auto last=std::min(static_cast<int>(peaks.maxima.size()),
            static_cast<int>(std::ceil(to*peaks.sampleRate/NativeRenderedPeaks::samplesPerBucket)));
        float peak=0;
        for(int i=first;i<last;++i)peak=std::max({peak,std::abs(peaks.minima[static_cast<std::size_t>(i)]),
                                                        std::abs(peaks.maxima[static_cast<std::size_t>(i)])});
        return peak;
    };
    drawNativePeakShape(g,durationSeconds,peakAt,bounds,opacity,selected,NativeWaveformTint::paleBlue,phase);
}

}
