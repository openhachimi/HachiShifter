#include "OtoWaveformEditorComponent.h"
#include "backend/UtauRenderer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <vector>

namespace hachi
{
namespace
{
juce::String utf8(const char* text) { return juce::String::fromUTF8(text); }

constexpr std::array<const char*, 8> parameterNames {
    "偏移", "辅音", "终止", "先行声音", "重叠",
    // The three Jie boundaries, named after the region each one closes.
    "声母末", "介音末", "韵腹末"
};

// Onset / glide / nucleus / coda, in the order the regions appear.  These
// are 界's names: four regions that are always the four parts of a Chinese
// syllable, in that order.
constexpr std::array<const char*, 4> jieRegionNames {
    "声母", "介音", "韵腹", "韵尾"
};

// What to call one region.  谋 numbers them: an entry there has two, three or
// four regions and any of them may be a consonant, so naming the last one 韵尾
// would be asserting a structure the entry is free to contradict -- and the
// number is what the tick boxes beside the wave are labelled with, so the two
// can be read against each other.
juce::String regionName(bool mou, int index)
{
    if (mou) return utf8("第") + juce::String(index + 1) + utf8("区");
    return juce::isPositiveAndBelow(index, 4)
        ? utf8(jieRegionNames[static_cast<std::size_t>(index)]) : juce::String{};
}

// A whole recording, as two channels: a mono file is copied to both, or it
// would come out of the left speaker only.  Empty when it cannot be read.
juce::AudioBuffer<float> readRecording(const juce::File& file, double& sampleRate)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0
        || reader->lengthInSamples > std::numeric_limits<int>::max())
        return {};
    sampleRate = reader->sampleRate;
    const auto count = static_cast<int>(reader->lengthInSamples);
    const auto channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> read(channels, count);
    read.clear();
    if (!reader->read(&read, 0, count, 0, true, channels > 1)) return {};
    juce::AudioBuffer<float> recording(2, count);
    for (int channel = 0; channel < 2; ++channel)
        recording.copyFrom(channel, 0, read, std::min(channel, channels - 1), 0, count);
    return recording;
}
}

// A recording, played once from the top at whatever rate the device runs, and
// silent after its end.  The audio thread only reads the samples and moves the
// position; the window reads the position back for the playhead, so the
// position is the one thing the two share.
class OtoWaveformEditorComponent::SourcePreview final : public juce::AudioSource
{
public:
    SourcePreview(juce::AudioBuffer<float> audio, double rate)
        : recording(std::move(audio)), recordingRate(rate) {}

    void prepareToPlay(int, double sampleRate) override
    {
        step.store(sampleRate > 0.0 ? recordingRate / sampleRate : 1.0);
    }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        render(*info.buffer, info.startSample, info.numSamples);
    }
    // Fills up to count samples from startSample and says how many there were
    // to give.  Past the end nothing is written.
    int render(juce::AudioBuffer<float>& buffer, int startSample, int count)
    {
        const auto length = recording.getNumSamples();
        const auto advance = step.load();
        auto at = position.load();
        auto written = 0;
        for (; written < count && at < static_cast<double>(length); ++written, at += advance)
        {
            const auto left = static_cast<int>(at);
            const auto right = std::min(length - 1, left + 1);
            const auto fraction = static_cast<float>(at - left);
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                const auto* samples = recording.getReadPointer(std::min(channel, 1));
                buffer.setSample(channel, startSample + written,
                    samples[left] + (samples[right] - samples[left]) * fraction);
            }
        }
        position.store(at);
        return written;
    }
    // How far in, in seconds.
    [[nodiscard]] double seconds() const
    {
        return std::min(position.load(), static_cast<double>(recording.getNumSamples()))
            / recordingRate;
    }
    [[nodiscard]] bool finished() const
    {
        return position.load() >= static_cast<double>(recording.getNumSamples());
    }

private:
    const juce::AudioBuffer<float> recording;
    const double recordingRate;
    std::atomic<double> position { 0.0 };
    std::atomic<double> step { 1.0 };
};

OtoWaveformEditorComponent::WaveformView::WaveformView(
    VoicebankOtoEntry& value, bool jieMode, bool mouMode,
    std::function<void()> changedCallback)
    : entry(value), jie(jieMode), mou(mouMode), changed(std::move(changedCallback))
{
    setOpaque(true);
    formats.registerBasicFormats();
    loadWaveform();
    // Open on the whole file.  Framing the entry instead put the rest of the
    // sample off screen, so the waveform looked like it ended where the view
    // did; anyone wanting a closer look can zoom in, and FIT still returns to
    // the entry's own range.
    showWholeFile();
}

void OtoWaveformEditorComponent::WaveformView::paint(juce::Graphics& g)
{
    g.fillAll(Palette::graphBackground);
    const auto plot = plotBounds();
    g.setColour(Palette::border);
    g.drawRect(plot);
    if (durationMs <= 0.0)
    {
        g.setColour(Palette::textMuted);
        g.drawFittedText(utf8("无法读取音频：") + entry.audioFile.getFullPathName(), plot.reduced(12),
                         juce::Justification::centred, 2);
        return;
    }

    const auto offsetX = xForMilliseconds(entry.offsetMs);
    const auto endX = xForMilliseconds(endMilliseconds());
    const auto consonantX = xForMilliseconds(entry.offsetMs + entry.consonantMs);
    const auto preutteranceX = xForMilliseconds(entry.offsetMs + entry.preutteranceMs);
    const auto overlapX = xForMilliseconds(entry.offsetMs + entry.overlapMs);

    g.setColour(juce::Colours::mediumpurple.withAlpha(0.27f));
    g.fillRect(juce::Rectangle<float>(static_cast<float>(plot.getX()),
        static_cast<float>(plot.getY()), std::max(0.0f, offsetX - plot.getX()),
        static_cast<float>(plot.getHeight())));
    g.fillRect(juce::Rectangle<float>(endX, static_cast<float>(plot.getY()),
        std::max(0.0f, plot.getRight() - endX), static_cast<float>(plot.getHeight())));

    if (!jie)
    {
        g.setColour(juce::Colours::red.withAlpha(0.18f));
        g.fillRect(juce::Rectangle<float>(std::min(offsetX, consonantX),
            static_cast<float>(plot.getY()), std::abs(consonantX - offsetX),
            static_cast<float>(plot.getHeight())));
    }

    // In Jie mode the four regions are shaded instead of the single consonant
    // block, so a collapsed region still reads as a visible seam.
    const auto jieOnsetX = xForMilliseconds(entry.offsetMs + entry.jieOnsetMs);
    const auto jieGlideX = xForMilliseconds(entry.offsetMs + entry.jieGlideMs);
    const auto jieNucleusX = xForMilliseconds(entry.offsetMs + entry.jieNucleusMs);
    const std::array<juce::Colour, 4> jieColours {
        juce::Colours::orangered, juce::Colours::gold,
        juce::Colours::deepskyblue, juce::Colours::mediumseagreen
    };
    // The spans this entry really has.  The last one always closes at the
    // end of the entry, so three regions use two inner boundaries and two
    // regions use one; the rest of the array is not drawn at all.
    const std::array<float, 3> inner { jieOnsetX, jieGlideX, jieNucleusX };
    const auto count = juce::jlimit(2, 4, regions);
    std::array<std::pair<float, float>, 4> spans {};
    {
        auto from = offsetX;
        for (int index = 0; index < count; ++index)
        {
            const auto to = index + 1 == count
                ? endX : inner[static_cast<std::size_t>(index)];
            spans[static_cast<std::size_t>(index)] = { from, to };
            from = to;
        }
    }
    if (jie)
    {
        for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index)
        {
            g.setColour(jieColours[index].withAlpha(0.17f));
            g.fillRect(juce::Rectangle<float>(spans[index].first,
                static_cast<float>(plot.getY()),
                std::max(0.0f, spans[index].second - spans[index].first),
                static_cast<float>(plot.getHeight())));
        }
    }

    if (!waveformPeaks.empty())
    {
        g.setColour(Palette::accentLight);
        const auto centreY = static_cast<float>(plot.getCentreY());
        const auto amplitude = static_cast<float>(plot.getHeight()) * 0.43f
            * static_cast<float>(amplitudeZoom);
        const auto columns = std::max(1, plot.getWidth());
        // Each column covers one slice of the visible window, not of the file.
        const auto peakAt = [this](double milliseconds)
        {
            const auto ratio = juce::jlimit(0.0, 1.0, milliseconds / std::max(1.0, durationMs));
            return static_cast<std::size_t>(juce::jlimit<double>(0.0,
                static_cast<double>(waveformPeaks.size() - 1),
                ratio * static_cast<double>(waveformPeaks.size() - 1)));
        };
        const auto spanMs = visibleSpanMs();
        const auto startMs = viewStartMs();
        for (int column = 0; column < columns; ++column)
        {
            const auto first = peakAt(startMs + spanMs * column / columns);
            const auto last = std::max(first + 1,
                peakAt(startMs + spanMs * (column + 1) / columns));
            auto minimum = 1.0f;
            auto maximum = -1.0f;
            for (auto index = first; index < std::min(last, waveformPeaks.size()); ++index)
            {
                minimum = std::min(minimum, waveformPeaks[index].first);
                maximum = std::max(maximum, waveformPeaks[index].second);
            }
            if (minimum > maximum) minimum = maximum = 0.0f;
            const auto x = static_cast<float>(plot.getX() + column);
            g.drawVerticalLine(static_cast<int>(x), centreY - maximum * amplitude,
                               centreY - minimum * amplitude);
        }
    }
    else
    {
        g.setColour(Palette::textMuted);
        g.drawText(utf8("音频中没有可显示的波形"), plot, juce::Justification::centred);
    }

    const auto drawBoundary = [&g, &plot](float x, juce::Colour colour, float thickness)
    {
        g.setColour(colour);
        g.drawLine(x, static_cast<float>(plot.getY()), x,
                   static_cast<float>(plot.getBottom()), thickness);
    };
    drawBoundary(offsetX, juce::Colours::mediumpurple.brighter(0.35f), 2.0f);
    drawBoundary(endX, juce::Colours::mediumpurple.brighter(0.35f), 2.0f);
    if (!jie) drawBoundary(consonantX, juce::Colours::orangered, 2.0f);
    drawBoundary(preutteranceX, juce::Colours::red, 2.0f);
    drawBoundary(overlapX, juce::Colours::limegreen, 2.0f);

    const std::array<std::pair<float, juce::String>, 5> labels {{
        { offsetX, utf8("偏移") }, { consonantX, utf8("辅音") },
        { endX, utf8("终止") }, { preutteranceX, utf8("先行") },
        { overlapX, utf8("重叠") }
    }};
    const std::array<juce::Colour, 5> colours {
        juce::Colours::mediumpurple.brighter(0.35f), juce::Colours::orangered,
        juce::Colours::mediumpurple.brighter(0.35f), juce::Colours::red,
        juce::Colours::limegreen
    };
    g.setFont(11.0f);
    for (std::size_t index = 0; index < labels.size(); ++index)
    {
        if (jie && index == 1) continue;   // the onset boundary is this line
        auto x = juce::jlimit(static_cast<float>(plot.getX()),
                              static_cast<float>(plot.getRight() - 38), labels[index].first - 18.0f);
        auto badge = juce::Rectangle<float>(x, static_cast<float>(2 + (index % 2) * 16), 38.0f, 15.0f);
        g.setColour(colours[index].withAlpha(0.78f));
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(colours[index].contrasting(0.9f));
        g.drawText(labels[index].second, badge, juce::Justification::centred);
    }

    if (jie)
    {
        for (int index = 0; index + 1 < count; ++index)
            drawBoundary(inner[static_cast<std::size_t>(index)],
                         jieColours[static_cast<std::size_t>(index) + 1].darker(0.15f),
                         2.0f);

        // Region names sit inside their own span, on a row below the oto
        // badges so the two sets never collide.
        for (std::size_t index = 0; index < static_cast<std::size_t>(count); ++index)
        {
            const auto width = spans[index].second - spans[index].first;
            if (width < 26.0f) continue;
            auto badge = juce::Rectangle<float>(
                spans[index].first + width * 0.5f - 17.0f,
                static_cast<float>(plot.getBottom() - 19), 34.0f, 15.0f);
            g.setColour(jieColours[index].withAlpha(0.82f));
            g.fillRoundedRectangle(badge, 3.0f);
            g.setColour(jieColours[index].contrasting(0.9f));
            g.drawText(regionName(mou, static_cast<int>(index)), badge,
                       juce::Justification::centred);
        }
    }

    g.setColour(Palette::textMuted);
    g.setFont(11.0f);
    for (int division = 0; division <= 10; ++division)
    {
        const auto value = viewStartMs() + visibleSpanMs() * division / 10.0;
        const auto x = xForMilliseconds(value);
        g.drawVerticalLine(static_cast<int>(x), static_cast<float>(plot.getY()),
                           static_cast<float>(plot.getY() + 5));
        g.drawText(juce::String(value / 1000.0, 3), static_cast<int>(x) - 28,
                   plot.getBottom() + 2, 56, 16, juce::Justification::centred);
    }

    // 播放原音: how far the sound has got, over everything else.
    if (playheadMs)
    {
        const auto x = xForMilliseconds(*playheadMs);
        if (x >= static_cast<float>(plot.getX()) && x <= static_cast<float>(plot.getRight()))
        {
            g.setColour(juce::Colours::white);
            g.drawLine(x, static_cast<float>(plot.getY()), x,
                       static_cast<float>(plot.getBottom()), 1.5f);
        }
    }
}

void OtoWaveformEditorComponent::WaveformView::mouseMove(const juce::MouseEvent& event)
{
    updateCursor(event.position);
}

void OtoWaveformEditorComponent::WaveformView::mouseExit(const juce::MouseEvent&)
{
    if (dragging == Handle::none) setMouseCursor(juce::MouseCursor::NormalCursor);
}

void OtoWaveformEditorComponent::WaveformView::mouseDown(const juce::MouseEvent& event)
{
    dragging = handleNear(event.position);
    // Nothing under the cursor: drag the view instead.  Without this, zooming
    // in would strand the user wherever the window happened to land.
    panning = dragging == Handle::none && viewZoom > 1.0;
    panStartCentreMs = viewCentreMs;
    updateCursor(event.position);
}

void OtoWaveformEditorComponent::WaveformView::mouseDrag(const juce::MouseEvent& event)
{
    if (panning && durationMs > 0.0)
    {
        const auto perPixel = visibleSpanMs() / std::max(1, plotBounds().getWidth());
        viewCentreMs = panStartCentreMs - event.getDistanceFromDragStartX() * perPixel;
        clampView();
        repaint();
        if (onViewChanged) onViewChanged();
        return;
    }
    if (dragging == Handle::none || durationMs <= 0.0) return;
    applyHandle(dragging, millisecondsForX(event.position.x));
}

void OtoWaveformEditorComponent::WaveformView::applyHandle(Handle handle, double value)
{
    if (handle == Handle::none || durationMs <= 0.0) return;
    switch (handle)
    {
        case Handle::offset:
        {
            const auto maximum = entry.cutoffMs < 0.0
                ? std::max(0.0, durationMs + entry.cutoffMs) : endMilliseconds();
            const auto previous = entry.offsetMs;
            entry.offsetMs = juce::jlimit(0.0, std::max(0.0, maximum - 0.1), value);
            // Every other field is a distance measured from the offset, so
            // moving it on its own carried the whole entry along.  Take the
            // move back out of each one so the boundaries keep the place in
            // the waveform they were put on, and only the offset line moves.
            const auto shift = entry.offsetMs - previous;
            if (std::abs(shift) > 1.0e-9)
            {
                entry.consonantMs = std::max(0.0, entry.consonantMs - shift);
                entry.preutteranceMs -= shift;
                entry.overlapMs -= shift;
                // A negative cutoff is a length from the offset and so moves
                // with it; a positive one counts back from the end of the file
                // and is already independent.
                if (entry.cutoffMs < 0.0) entry.cutoffMs += shift;
                entry.jieOnsetMs = std::max(0.0, entry.jieOnsetMs - shift);
                entry.jieGlideMs = std::max(entry.jieOnsetMs, entry.jieGlideMs - shift);
                entry.jieNucleusMs = std::max(entry.jieGlideMs,
                                              entry.jieNucleusMs - shift);
            }
            break;
        }
        case Handle::consonant:
            entry.consonantMs = std::max(0.0, value - entry.offsetMs);
            break;
        case Handle::cutoff:
            value = juce::jlimit(entry.offsetMs + 0.1, durationMs, value);
            entry.cutoffMs = entry.cutoffMs < 0.0
                ? -(value - entry.offsetMs) : durationMs - value;
            break;
        case Handle::preutterance:
            entry.preutteranceMs = value - entry.offsetMs;
            break;
        case Handle::overlap:
            entry.overlapMs = value - entry.offsetMs;
            break;
        // The Jie boundaries stay ordered and inside [offset, end], so a
        // drag can collapse a region to zero but never turn it inside out.
        case Handle::jieOnset:
            entry.jieOnsetMs = juce::jlimit(0.0, boundaryCeiling(0),
                                            value - entry.offsetMs);
            entry.consonantMs = entry.jieOnsetMs;
            break;
        case Handle::jieGlide:
            entry.jieGlideMs = juce::jlimit(entry.jieOnsetMs, boundaryCeiling(1),
                                            value - entry.offsetMs);
            break;
        case Handle::jieNucleus:
            entry.jieNucleusMs = juce::jlimit(entry.jieGlideMs,
                std::max(entry.jieGlideMs, boundaryCeiling(2)),
                value - entry.offsetMs);
            break;
        case Handle::none: break;
    }
    orderBoundaries();
    if (changed) changed();
    repaint();
}

void OtoWaveformEditorComponent::WaveformView::mouseUp(const juce::MouseEvent& event)
{
    dragging = Handle::none;
    panning = false;
    updateCursor(event.position);
}

void OtoWaveformEditorComponent::WaveformView::loadWaveform()
{
    waveformPeaks.clear();
    if (!entry.audioFile.existsAsFile()) return;
    auto reader = std::unique_ptr<juce::AudioFormatReader>(
        formats.createReaderFor(entry.audioFile));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
        return;

    durationMs = static_cast<double>(reader->lengthInSamples)
        / reader->sampleRate * 1000.0;
    constexpr int peakCount = 2048;
    constexpr int chunkSize = 8192;
    waveformPeaks.assign(peakCount, { 1.0f, -1.0f });
    const auto channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> buffer(channels, chunkSize);
    for (juce::int64 position = 0; position < reader->lengthInSamples; position += chunkSize)
    {
        const auto amount = static_cast<int>(std::min<juce::int64>(
            chunkSize, reader->lengthInSamples - position));
        buffer.clear();
        if (!reader->read(&buffer, 0, amount, position, true, channels > 1))
            continue;
        for (int sample = 0; sample < amount; ++sample)
        {
            const auto absolute = position + sample;
            const auto peakIndex = static_cast<std::size_t>(std::min<juce::int64>(
                peakCount - 1, absolute * peakCount / reader->lengthInSamples));
            auto minimum = 1.0f;
            auto maximum = -1.0f;
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto value = buffer.getSample(channel, sample);
                minimum = std::min(minimum, value);
                maximum = std::max(maximum, value);
            }
            waveformPeaks[peakIndex].first = std::min(waveformPeaks[peakIndex].first, minimum);
            waveformPeaks[peakIndex].second = std::max(waveformPeaks[peakIndex].second, maximum);
        }
    }
    for (auto& peak : waveformPeaks)
        if (peak.first > peak.second) peak = { 0.0f, 0.0f };
}

juce::Rectangle<int> OtoWaveformEditorComponent::WaveformView::plotBounds() const
{
    auto bounds = getLocalBounds().reduced(14, 2);
    bounds.removeFromTop(36);
    bounds.removeFromBottom(22);
    return bounds;
}

double OtoWaveformEditorComponent::WaveformView::visibleSpanMs() const
{
    // Below 1 the window is wider than the file, which is how a handle that
    // sits outside it -- a large overlap, a preutterance past the end -- is
    // brought on screen at all.
    return durationMs / std::max(0.01, viewZoom);
}

double OtoWaveformEditorComponent::WaveformView::viewStartMs() const
{
    return viewCentreMs - visibleSpanMs() * 0.5;
}

void OtoWaveformEditorComponent::WaveformView::clampView()
{
    // Half of 1x is two clicks of the zoom-out button, enough margin either
    // side of the file for an oto value that falls outside it.
    viewZoom = juce::jlimit(0.5, 400.0, viewZoom);
    amplitudeZoom = juce::jlimit(0.25, 16.0, amplitudeZoom);
    const auto span = visibleSpanMs();
    if (span >= durationMs)
        // Wider than the file: centre it, so the margin is shared rather than
        // all of it landing on one side.
        viewCentreMs = durationMs * 0.5;
    else
        // Zoomed in, the window may not run past either end of the file.
        viewCentreMs = juce::jlimit(span * 0.5, durationMs - span * 0.5, viewCentreMs);
}

void OtoWaveformEditorComponent::WaveformView::nudgeHorizontalZoom(double factor)
{
    viewZoom *= factor;
    clampView();
    repaint();
    if (onViewChanged) onViewChanged();
}

void OtoWaveformEditorComponent::WaveformView::setVisibleStartMilliseconds(double startMs)
{
    viewCentreMs = startMs + visibleSpanMs() * 0.5;
    clampView();
    repaint();
}

void OtoWaveformEditorComponent::WaveformView::nudgeVerticalZoom(double factor)
{
    amplitudeZoom *= factor;
    clampView();
    repaint();
}

void OtoWaveformEditorComponent::WaveformView::showWholeFile()
{
    viewZoom = 1.0;
    viewCentreMs = durationMs * 0.5;
    clampView();
    repaint();
    if (onViewChanged) onViewChanged();
}

void OtoWaveformEditorComponent::WaveformView::frameEntry()
{
    // Fit the oto region with a little air on both sides.
    const auto span = std::max(1.0, endMilliseconds() - entry.offsetMs);
    const auto padded = span * 1.25;
    viewCentreMs = entry.offsetMs + span * 0.5;
    viewZoom = durationMs > 0.0 ? std::max(1.0, durationMs / padded) : 1.0;
    clampView();
    repaint();
    if (onViewChanged) onViewChanged();
}

float OtoWaveformEditorComponent::WaveformView::xForMilliseconds(double milliseconds) const
{
    const auto plot = plotBounds();
    if (durationMs <= 0.0) return static_cast<float>(plot.getX());
    const auto span = std::max(1.0e-9, visibleSpanMs());
    // Not clamped to the plot: boundary lines outside the window are drawn off
    // to the side and clipped, rather than piling up on the edge.
    return static_cast<float>(plot.getX()) + static_cast<float>(plot.getWidth())
        * static_cast<float>((milliseconds - viewStartMs()) / span);
}

double OtoWaveformEditorComponent::WaveformView::millisecondsForX(float x) const
{
    const auto plot = plotBounds();
    const auto ratio = static_cast<double>(x - plot.getX()) / std::max(1, plot.getWidth());
    return juce::jlimit(0.0, durationMs, viewStartMs() + ratio * visibleSpanMs());
}

double OtoWaveformEditorComponent::WaveformView::endMilliseconds() const
{
    return entry.cutoffMs < 0.0 ? entry.offsetMs - entry.cutoffMs
                                : durationMs - entry.cutoffMs;
}

double OtoWaveformEditorComponent::WaveformView::boundaryCeiling(int index) const
{
    const auto span = std::max(0.0, endMilliseconds() - entry.offsetMs);
    if (index + 1 >= boundaryCount()) return span;
    return index == 0 ? entry.jieGlideMs : entry.jieNucleusMs;
}

void OtoWaveformEditorComponent::WaveformView::orderBoundaries()
{
    entry.jieGlideMs = std::max(entry.jieGlideMs, entry.jieOnsetMs);
    entry.jieNucleusMs = std::max(entry.jieNucleusMs, entry.jieGlideMs);
}

OtoWaveformEditorComponent::WaveformView::Handle
OtoWaveformEditorComponent::WaveformView::handleNear(const juce::Point<float>& position) const
{
    if (durationMs <= 0.0 || !plotBounds().expanded(8, 32).contains(position.toInt()))
        return Handle::none;
    std::vector<std::pair<Handle, float>> handles {
        { Handle::preutterance, xForMilliseconds(entry.offsetMs + entry.preutteranceMs) },
        { Handle::overlap, xForMilliseconds(entry.offsetMs + entry.overlapMs) },
        { Handle::offset, xForMilliseconds(entry.offsetMs) },
        { Handle::cutoff, xForMilliseconds(endMilliseconds()) }
    };
    // Only classic mode has a consonant of its own; in Jie mode the onset
    // boundary is that same line and there is nothing to disambiguate.
    if (!jie)
        handles.push_back({ Handle::consonant,
            xForMilliseconds(entry.offsetMs + entry.consonantMs) });
    if (jie)
    {
        // Only the boundaries this entry has.  The ones past the count are not
        // drawn, so a press that took hold of one would be pulling on a line
        // nobody can see.
        const auto shown = boundaryCount();
        // Ties go to whichever handle was pushed first.  Where the onset and
        // the glide sit on the same millisecond -- which is how every entry
        // seeded from a classic oto starts out -- only the glide can still
        // move: the onset is clamped by it and would not budge, leaving the
        // pair permanently welded together.
        const auto glideOnOnset =
            std::abs(entry.jieGlideMs - entry.jieOnsetMs) < 1.0e-6;
        if (glideOnOnset && shown > 1)
            handles.push_back({ Handle::jieGlide,
                xForMilliseconds(entry.offsetMs + entry.jieGlideMs) });
        if (shown > 0)
            handles.push_back({ Handle::jieOnset,
                xForMilliseconds(entry.offsetMs + entry.jieOnsetMs) });
        if (!glideOnOnset && shown > 1)
            handles.push_back({ Handle::jieGlide,
                xForMilliseconds(entry.offsetMs + entry.jieGlideMs) });
        if (shown > 2)
            handles.push_back({ Handle::jieNucleus,
                xForMilliseconds(entry.offsetMs + entry.jieNucleusMs) });
    }
    auto nearest = Handle::none;
    auto nearestDistance = 9.0f;
    for (const auto& handle : handles)
    {
        const auto distance = std::abs(position.x - handle.second);
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = handle.first;
        }
    }
    return nearest;
}

juce::String OtoWaveformEditorComponent::WaveformView::diagnosticHandleAtX(float x) const
{
    const auto middle = plotBounds().getCentreY();
    switch (handleNear({ x, static_cast<float>(middle) }))
    {
        case Handle::offset:      return "offset";
        case Handle::consonant:   return "consonant";
        case Handle::cutoff:      return "cutoff";
        case Handle::preutterance:return "preutterance";
        case Handle::overlap:     return "overlap";
        case Handle::jieOnset:    return "b1";
        case Handle::jieGlide:    return "b2";
        case Handle::jieNucleus:  return "b3";
        case Handle::none:        break;
    }
    return "none";
}

void OtoWaveformEditorComponent::WaveformView::updateCursor(
    const juce::Point<float>& position)
{
    if (dragging != Handle::none || handleNear(position) != Handle::none)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(viewZoom > 1.0 ? juce::MouseCursor::DraggingHandCursor
                                      : juce::MouseCursor::NormalCursor);
}

OtoWaveformEditorComponent::OtoWaveformEditorComponent(
    VoicebankOtoEntry entry, bool jieMode, bool mouMode,
    std::function<void()> savedCallback)
    : jie(jieMode), mou(mouMode), original(entry), edited(std::move(entry)),
      onSaved(std::move(savedCallback)),
      waveform(edited, jieMode, mouMode, [this] { refreshEditors(); })
{
    setOpaque(true);
    // Opening an entry the voicebank has not been extended for yet: start from
    // the classic layout written as four regions, so the boundaries exist and
    // can be dragged apart rather than appearing stacked at zero.
    if (jie)
    {
        const auto span = std::max(0.0, waveform.entrySpanMilliseconds());
        if (!edited.hasJieOto)
        {
            // Extending a classic entry: the onset is its consonant and the
            // glide starts empty, the way seeding a whole voicebank writes it.
            edited.jieOnsetMs = juce::jlimit(0.0, span, edited.consonantMs);
            edited.jieGlideMs = edited.jieOnsetMs;
            edited.jieNucleusMs = span;
        }
        if (edited.jieOnsetMs <= 1.0e-6 && edited.jieGlideMs <= 1.0e-6)
        {
            // Nothing has been marked: a sample with no consonant to seed
            // from, or one already saved while still in that state.  Every
            // inner boundary sits on zero, stacking the handles on the left
            // edge where none of them can be told apart or grabbed.  Quarters
            // give each region a span wide enough to drag from.  Checked here
            // rather than only while seeding, because saving once makes the
            // entry look extended while leaving it just as unusable.
            edited.jieOnsetMs = span * 0.25;
            edited.jieGlideMs = span * 0.50;
            edited.jieNucleusMs = span * 0.75;
        }
    }
    fileLabel.setText(edited.sourceName + (edited.alias.isNotEmpty()
        ? utf8("  ·  别名：") + edited.alias : juce::String{}), juce::dontSendNotification);
    fileLabel.setFont(fileLabel.getFont().boldened());
    fileLabel.setTooltip(edited.audioFile.getFullPathName());
    addAndMakeVisible(fileLabel);

    helpLabel.setText(utf8("拖动彩色边界或线条调整参数；数值单位均为 ms"),
                      juce::dontSendNotification);
    helpLabel.setColour(juce::Label::textColourId, Palette::textMuted);
    helpLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(helpLabel);

    // Every label, not only the ones on screen now: a two-region entry opens
    // with the last two hidden, and they used to become visible with no text
    // at all the moment the count grew.
    for (std::size_t index = 0; index < parameterNames.size(); ++index)
        parameterLabels[index].setText(
            index >= 5 && mou
                ? utf8("第") + juce::String(static_cast<int>(index) - 4) + utf8("区末")
                : utf8(parameterNames[index]),
            juce::dontSendNotification);
    // Built for every parameter, not only the ones this entry opens with: an
    // entry that opens at two regions used to leave the last boundaries out of
    // the component altogether, so growing it to four showed nothing where
    // they should have been.
    for (std::size_t index = 0; index < parameterEditors.size(); ++index)
    {
        parameterLabels[index].setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(parameterLabels[index]);
        parameterEditors[index].setInputRestrictions(16, "0123456789.-");
        parameterEditors[index].setSelectAllWhenFocused(true);
        parameterEditors[index].onFocusLost = [this] { commitEditors(); };
        parameterEditors[index].onReturnKey = [this] { commitEditors(); };
        addAndMakeVisible(parameterEditors[index]);
    }
    applyParameterVisibility();
    addAndMakeVisible(waveform);

    // Button captions stay ASCII: the axis letter tells the two pairs apart
    // at 24 px, and the Chinese explanation lives in the tooltip.
    struct ZoomButton { juce::TextButton* button; const char* text; const char* tip; };
    const std::array<ZoomButton, 5> zoomButtons {{
        { &hZoomInButton,   "H+",  "横向放大（时间轴）" },
        { &hZoomOutButton,  "H-",  "横向缩小（时间轴）" },
        { &vZoomInButton,   "V+",  "纵向放大（波形振幅）" },
        { &vZoomOutButton,  "V-",  "纵向缩小（波形振幅）" },
        { &zoomResetButton, "FIT", "回到本条目的取样范围" }
    }};
    for (const auto& item : zoomButtons)
    {
        item.button->setButtonText(item.text);
        item.button->setTooltip(utf8(item.tip));
        addAndMakeVisible(*item.button);
    }
    hZoomInButton.onClick  = [this] { waveform.nudgeHorizontalZoom(1.4); };
    hZoomOutButton.onClick = [this] { waveform.nudgeHorizontalZoom(1.0 / 1.4); };
    vZoomInButton.onClick  = [this] { waveform.nudgeVerticalZoom(1.4); };
    vZoomOutButton.onClick = [this] { waveform.nudgeVerticalZoom(1.0 / 1.4); };
    zoomResetButton.onClick = [this] { waveform.frameEntry(); };

    waveformScroll.setAutoHide(false);
    waveformScroll.addListener(this);
    addAndMakeVisible(waveformScroll);
    // The bar mirrors whatever moved the view, whether that was a zoom button
    // or dragging the waveform itself.
    waveform.onViewChanged = [this] { refreshScrollBar(); };

    saveButton.setButtonText(mou ? utf8("保存谋•OTO（独立）")
                             : jie ? utf8("保存界•OTO（独立）")
                                   : utf8("保存到 oto.ini"));
    if (mou)
    {
        // An entry with nothing written for it opens at CVVV -- which is what
        // an oto4 row has always meant -- rather than at an empty string the
        // field shows as CVVV and the count reads as four.  The two have to
        // agree from the first frame, or picking "4 段分" appears to do
        // nothing because it already thinks it is there.
        if (edited.mouClasses.trim().length() < 2) edited.mouClasses = "CVVV";
        classesLabel.setText(utf8("音素类别"), juce::dontSendNotification);
        classesLabel.setTooltip(utf8("每区一个字母：C 辅音 / V 元音 / S 静音。"
                                     "串长就是区数（2~4），字母说明哪一段是元音。"));
        addAndMakeVisible(classesLabel);
        classesEditor.setText(edited.mouClasses.isNotEmpty() ? edited.mouClasses
                                                             : juce::String("CVVV"),
                              false);
        classesEditor.setTooltip(classesLabel.getTooltip());
        classesEditor.onTextChange = [this]
        {
            // Only ever letters this means something in, upper case, and never
            // longer than there are regions.
            auto text = classesEditor.getText().toUpperCase()
                            .retainCharacters("CVS").substring(0, 4);
            if (text != classesEditor.getText())
                classesEditor.setText(text, false);
            if (text.length() >= 2)
            {
                edited.mouClasses = text;
                syncRegionCount();
                resized();
                refreshEditors();
            }
        };
        addAndMakeVisible(classesEditor);
        consonantLabel.setText(utf8("辅音区"), juce::dontSendNotification);
        consonantLabel.setTooltip(utf8("勾选哪几个区是辅音，可以一个不勾，也可以勾多个。"
                                       "勾上的区不参与主动拉伸，并且走辅音那套合成。"));
        addAndMakeVisible(consonantLabel);
        for (int index = 0; index < 4; ++index)
        {
            auto& tick = consonantButtons[static_cast<std::size_t>(index)];
            tick.setButtonText(utf8("第") + juce::String(index + 1) + utf8("区"));
            tick.setTooltip(consonantLabel.getTooltip());
            tick.onClick = [this, index]
            {
                setRegionIsConsonant(index,
                    consonantButtons[static_cast<std::size_t>(index)].getToggleState());
            };
            addAndMakeVisible(tick);
        }
        for (int index = 0; index < 3; ++index)
        {
            const auto count = index + 2;
            auto& button = countButtons[static_cast<std::size_t>(index)];
            button.setButtonText(juce::String(count) + utf8(" 段分"));
            button.setTooltip(utf8("把这条 oto 切成 ") + juce::String(count)
                              + utf8(" 段；波形与下面的边界数值会立刻跟着变"));
            button.setColour(juce::TextButton::buttonOnColourId, Palette::accent);
            button.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            button.setClickingTogglesState(true);
            button.setRadioGroupId(0x6d6f75);
            button.onClick = [this, count] { setRegionCount(count); };
            addAndMakeVisible(button);
        }
        syncRegionCount();
    }
    // 播放原音 sits at the other end of the row from Save and Cancel: listening
    // is not a way of leaving the window.
    playButton.setButtonText(utf8("播放原音"));
    playButton.setTooltip(utf8("从头播放这条 oto 所在的整个 wav 原始录音，"
                               "没经过任何引擎；再点一次停止"));
    playButton.onClick = [this] { togglePlayback(); };
    addAndMakeVisible(playButton);
    saveButton.onClick = [this] { save(); };
    addAndMakeVisible(saveButton);
    cancelButton.setButtonText(utf8("取消"));
    cancelButton.onClick = [this] { closeWindow(); };
    addAndMakeVisible(cancelButton);

    setSize(980, 500);
    refreshEditors();
}

void OtoWaveformEditorComponent::paint(juce::Graphics& g)
{
    g.fillAll(Palette::panel);
}

void OtoWaveformEditorComponent::scrollBarMoved(juce::ScrollBar* bar, double newRangeStart)
{
    if (bar != &waveformScroll) return;
    waveform.setVisibleStartMilliseconds(newRangeStart);
}

void OtoWaveformEditorComponent::refreshScrollBar()
{
    const auto total = std::max(1.0, waveform.totalMilliseconds());
    const auto visible = waveform.visibleLengthMilliseconds();
    waveformScroll.setRangeLimits({ 0.0, total }, juce::dontSendNotification);
    waveformScroll.setCurrentRange({ waveform.visibleStartMilliseconds(),
                                     waveform.visibleStartMilliseconds() + visible },
                                   juce::dontSendNotification);
    // Nothing to scroll to while the whole file is on screen.
    waveformScroll.setEnabled(visible < total - 1.0e-6);
}

void OtoWaveformEditorComponent::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto title = area.removeFromTop(28);
    fileLabel.setBounds(title.removeFromLeft(title.getWidth() / 2));
    helpLabel.setBounds(title);
    area.removeFromTop(6);
    auto parameters = area.removeFromTop(34);
    // The class string sits at the left of the parameter row, ahead of the
    // numbers: it says how many of them mean anything.
    if (mou)
    {
        classesLabel.setBounds(parameters.removeFromLeft(64));
        classesEditor.setBounds(parameters.removeFromLeft(72).reduced(3, 2));
        parameters.removeFromLeft(8);
    }
    const auto order = visibleParameters();
    const auto itemWidth = std::max(108,
        parameters.getWidth() / static_cast<int>(order.size()));
    for (std::size_t position = 0; position < order.size(); ++position)
    {
        const auto index = order[position];
        auto item = position + 1 == order.size() ? parameters
                                                 : parameters.removeFromLeft(itemWidth);
        parameterLabels[index].setBounds(item.removeFromLeft(jie ? 56 : 60));
        parameterEditors[index].setBounds(item.reduced(3, 2));
    }
    area.removeFromTop(7);
    auto buttons = area.removeFromBottom(34);
    playButton.setBounds(buttons.removeFromLeft(112).reduced(0, 2));
    cancelButton.setBounds(buttons.removeFromRight(92).reduced(0, 2));
    buttons.removeFromRight(8);
    saveButton.setBounds(buttons.removeFromRight(142).reduced(0, 2));
    area.removeFromBottom(8);
    // Zoom column down the right-hand side: horizontal pair, vertical pair,
    // then "fit to this entry".
    if (mou)
    {
        auto countColumn = area.removeFromRight(82);
        area.removeFromRight(4);
        countColumn.removeFromTop(2);
        for (auto& button : countButtons)
        {
            button.setBounds(countColumn.removeFromTop(26).reduced(0, 2));
            countColumn.removeFromTop(2);
        }
        // The ticks sit under the counts: how many regions there are, then
        // which of them are consonants.
        countColumn.removeFromTop(12);
        consonantLabel.setBounds(countColumn.removeFromTop(20));
        for (auto& tick : consonantButtons)
            tick.setBounds(countColumn.removeFromTop(24).reduced(2, 1));
    }
    // Wide enough for the captions once JUCE has taken its indents out of
     // both edges: at 26 the three-letter one had ten pixels to live in and
     // came out as an ellipsis.  Checked, not guessed -- see
     // diagnosticZoomCaptionsFit.
    auto zoomColumn = area.removeFromRight(44);
    area.removeFromRight(4);
    waveformScroll.setBounds(area.removeFromBottom(12));
    waveform.setBounds(area);
    refreshScrollBar();
    zoomColumn.removeFromTop(36);            // line up with the plot, not the badges
    const auto cell = 24;
    hZoomInButton.setBounds(zoomColumn.removeFromTop(cell).reduced(1));
    hZoomOutButton.setBounds(zoomColumn.removeFromTop(cell).reduced(1));
    zoomColumn.removeFromTop(8);
    vZoomInButton.setBounds(zoomColumn.removeFromTop(cell).reduced(1));
    vZoomOutButton.setBounds(zoomColumn.removeFromTop(cell).reduced(1));
    zoomColumn.removeFromTop(8);
    zoomResetButton.setBounds(zoomColumn.removeFromTop(cell).reduced(1));
}

namespace
{
// What LookAndFeel_V2::drawButtonText leaves for the text, worked out the same
// way it does: a font from the button height, then an indent at each edge.
int buttonTextRoom(const juce::TextButton& button)
{
    auto& look = const_cast<juce::TextButton&>(button).getLookAndFeel();
    const auto font = look.getTextButtonFont(const_cast<juce::TextButton&>(button),
                                             button.getHeight());
    const auto fontHeight = juce::roundToInt(font.getHeight() * 0.6f);
    const auto cornerSize = juce::jmin(button.getHeight(), button.getWidth()) / 2;
    const auto left = juce::jmin(fontHeight,
        2 + cornerSize / (button.isConnectedOnLeft() ? 4 : 2));
    const auto right = juce::jmin(fontHeight,
        2 + cornerSize / (button.isConnectedOnRight() ? 4 : 2));
    return button.getWidth() - left - right;
}

int buttonTextWidth(const juce::TextButton& button)
{
    auto& look = const_cast<juce::TextButton&>(button).getLookAndFeel();
    return look.getTextButtonFont(const_cast<juce::TextButton&>(button),
                                  button.getHeight())
        .getStringWidth(button.getButtonText());
}
}  // namespace

bool OtoWaveformEditorComponent::diagnosticZoomCaptionsFit(juce::String* report) const
{
    const std::array<const juce::TextButton*, 5> buttons {
        &hZoomInButton, &hZoomOutButton, &vZoomInButton, &vZoomOutButton,
        &zoomResetButton };
    auto fits = true;
    for (const auto* button : buttons)
    {
        const auto room = buttonTextRoom(*button);
        const auto width = buttonTextWidth(*button);
        fits = fits && width <= room;
        if (report != nullptr)
            *report += " " + button->getButtonText() + ":" + juce::String(width)
                + "/" + juce::String(room);
    }
    return fits;
}

int OtoWaveformEditorComponent::regionCount() const
{
    return mou ? SampleSettings::mouRegionCount(edited.mouClasses) : 4;
}

void OtoWaveformEditorComponent::setRegionCount(int count)
{
    count = juce::jlimit(2, 4, count);
    if (!mou || count == regionCount()) return;
    edited.mouClasses = SampleSettings::mouClassesForCount(edited.mouClasses, count);
    // The boundaries this count does not reach keep their times.  They are no
    // longer drawn, grabbable or read back, so they cost nothing here and are
    // still there if the entry is split the other way again -- which is what
    // the voicebank panel's buttons do too.
    classesEditor.setText(edited.mouClasses, false);
    syncRegionCount();
    resized();
    refreshEditors();
}

bool OtoWaveformEditorComponent::regionIsConsonant(int index) const
{
    const auto classes = edited.mouClasses.trim().toUpperCase();
    return juce::isPositiveAndBelow(index, classes.length())
        && classes[index] == 'C';
}

void OtoWaveformEditorComponent::setRegionIsConsonant(int index, bool consonant)
{
    if (!mou || !juce::isPositiveAndBelow(index, regionCount())) return;
    auto classes = SampleSettings::mouClassesForCount(edited.mouClasses,
                                                      regionCount());
    if (consonant == (classes[index] == 'C')) return;
    // Untick a silence and it stays a silence: the boxes only ever say
    // consonant or not, and throwing an S away would be answering a question
    // they never asked.
    juce::String next;
    for (int position = 0; position < classes.length(); ++position)
        next += position != index ? classes[position]
              : consonant         ? juce::juce_wchar('C')
              : classes[position] == 'S' ? juce::juce_wchar('S')
                                         : juce::juce_wchar('V');
    edited.mouClasses = next;
    classesEditor.setText(edited.mouClasses, false);
    syncRegionCount();
    refreshEditors();
}

juce::String OtoWaveformEditorComponent::diagnosticRegionName(int index) const
{
    return regionName(mou, index);
}

void OtoWaveformEditorComponent::applyParameterVisibility()
{
    const auto shown = visibleParameters();
    for (std::size_t index = 0; index < parameterEditors.size(); ++index)
    {
        const auto visible = std::find(shown.begin(), shown.end(), index) != shown.end();
        parameterLabels[index].setVisible(visible);
        parameterEditors[index].setVisible(visible);
    }
}

void OtoWaveformEditorComponent::syncRegionCount()
{
    waveform.regions = regionCount();
    waveform.classes = edited.mouClasses.trim().toUpperCase();
    for (int index = 0; index < 3; ++index)
        countButtons[static_cast<std::size_t>(index)].setToggleState(
            index + 2 == regionCount(), juce::dontSendNotification);
    // A region the count does not reach cannot be marked anything.
    for (int index = 0; index < 4; ++index)
    {
        auto& tick = consonantButtons[static_cast<std::size_t>(index)];
        tick.setEnabled(index < regionCount());
        tick.setToggleState(index < regionCount() && regionIsConsonant(index),
                            juce::dontSendNotification);
    }
    applyParameterVisibility();
    waveform.repaint();
}

void OtoWaveformEditorComponent::refreshEditors()
{
    refreshingEditors = true;
    const std::array<double, 8> values { edited.offsetMs, edited.consonantMs,
        edited.cutoffMs, edited.preutteranceMs, edited.overlapMs,
        edited.jieOnsetMs, edited.jieGlideMs, edited.jieNucleusMs };
    for (std::size_t index = 0; index < parameterEditors.size(); ++index)
        parameterEditors[index].setText(formatNumber(values[index]), false);
    refreshingEditors = false;
    waveform.repaint();
}

void OtoWaveformEditorComponent::commitEditors()
{
    if (refreshingEditors) return;
    edited.offsetMs = parameterEditors[0].getText().getDoubleValue();
    if (!jie) edited.consonantMs = parameterEditors[1].getText().getDoubleValue();
    edited.cutoffMs = parameterEditors[2].getText().getDoubleValue();
    edited.preutteranceMs = parameterEditors[3].getText().getDoubleValue();
    edited.overlapMs = parameterEditors[4].getText().getDoubleValue();
    if (jie)
    {
        // Same ordering rule the drag handles enforce, applied to typed
        // values -- and to the same boundaries.  A hidden editor still holds
        // the text it was last given, so reading all three would let a
        // boundary the entry does not have clamp the ones it does.
        const auto span = std::max(0.0, waveform.entrySpanMilliseconds());
        const auto count = regionCount();
        const std::array<double*, 3> bounds { &edited.jieOnsetMs,
                                              &edited.jieGlideMs,
                                              &edited.jieNucleusMs };
        for (int index = count - 2; index >= 0; --index)
            *bounds[static_cast<std::size_t>(index)] = juce::jlimit(0.0,
                index + 2 < count ? *bounds[static_cast<std::size_t>(index) + 1] : span,
                parameterEditors[static_cast<std::size_t>(5 + index)]
                    .getText().getDoubleValue());
        edited.jieGlideMs = std::max(edited.jieGlideMs, edited.jieOnsetMs);
        edited.jieNucleusMs = std::max(edited.jieNucleusMs, edited.jieGlideMs);
        // The classic row still carries a consonant: the two-region fallback
        // and the host's own timing read it.  It is simply no longer edited
        // separately -- the onset boundary is what it means here.
        edited.consonantMs = edited.jieOnsetMs;
    }
    refreshEditors();
}

void OtoWaveformEditorComponent::save()
{
    commitEditors();
    if (noteHandoff)
    {
        // 单独OTO编辑: the entry goes to one note and nowhere else.  No oto file
        // is written, and the voicebank's cached entries are left alone,
        // because nothing about the voicebank has changed.  谋's classes are
        // in the entry already: the box writes each valid string it holds
        // straight into it.
        noteHandoff(edited);
        closeWindow();
        return;
    }
    juce::String error;
    // A from-scratch material is stored natively: the override writes the HJM
    // sidecar and the oto files are left untouched.
    if (saveOverride)
    {
        if (!saveOverride(edited, error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                utf8("无法保存原生标注"), error);
            return;
        }
        backend::UtauRenderer::invalidateVoicebankCache();
        if (onSaved) onSaved();
        closeWindow();
        return;
    }
    const auto savedClassicTiming = jie
        ? SampleSettings::updateJieVoicebankOtoEntry(original, edited, error)
        : SampleSettings::updateVoicebankOtoEntry(original, edited, error);
    if (!savedClassicTiming)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            jie ? utf8("无法保存独立界•OTO") : utf8("无法保存 oto.ini"), error);
        return;
    }
    // The four-region row is keyed by the independent classic offset just
    // saved above.  It never writes the original oto.ini.
    // 谋 writes its own file; 界 writes oto4.ini.  A 谋 track never touches
    // oto4.ini, so annotating a voicebank cannot disturb what 界 renders.
    if (mou)
    {
        edited.mouClasses = classesEditor.getText().trim().toUpperCase();
        if (edited.mouClasses.length() < 2) edited.mouClasses = "CVVV";
        if (!SampleSettings::updateMouOtoEntry(original, edited, error))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                utf8("无法保存谋•OTO（otomou.ini）"), error);
            return;
        }
    }
    else if (jie && !SampleSettings::updateJieOtoEntry(original, edited, error))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            utf8("无法保存界•OTO（oto4.ini）"), error);
        return;
    }
    backend::UtauRenderer::invalidateVoicebankCache();
    if (onSaved) onSaved();
    closeWindow();
}

std::unique_ptr<OtoWaveformEditorComponent> OtoWaveformEditorComponent::forNote(
    ProjectModel& project, const juce::String& noteId, juce::String& error)
{
    const auto data = project.snapshot();
    const TrackData* owner = nullptr;
    const NoteData* target = nullptr;
    for (const auto& track : data.tracks)
        for (const auto& clip : track.clips)
            for (const auto& note : clip.notes)
                if (note.id == noteId) { owner = &track; target = &note; }
    if (owner == nullptr || target == nullptr) return {};
    if (owner->pitchAlgorithm != PitchAlgorithm::utau)
    {
        error = utf8("单独OTO编辑仅用于 UTAU 轨道。");
        return {};
    }
    if (!owner->voicebankDirectory.isDirectory())
    {
        error = utf8("请先为当前轨道选择 UTAU 音源库。");
        return {};
    }
    juce::StringArray warnings;
    const auto mouMode = owner->utauMode == UtauMode::mou;
    const auto regions = utauModeUsesRegions(owner->utauMode);
    const auto entries = SampleSettings::loadVoicebankOto(owner->voicebankDirectory,
                                                          warnings, regions, mouMode);
    const auto index = SampleSettings::findEntryForAlias(entries, target->label);
    if (index < 0)
    {
        error = utf8("音源库里没有这个歌词的 oto 条目。");
        return {};
    }
    auto editor = std::make_unique<OtoWaveformEditorComponent>(
        SampleSettings::entryWithNoteOto(entries[static_cast<std::size_t>(index)],
                                         target->utauOto),
        regions, mouMode, [] {});
    // The project outlives every window opened on it.
    editor->saveToNoteInstead([&project, noteId, regions](const VoicebankOtoEntry& entry)
    {
        project.setNoteUtauOto(noteId, SampleSettings::noteOtoFromEntry(entry, regions));
    });
    return editor;
}

void OtoWaveformEditorComponent::saveToNoteInstead(
    std::function<void(const VoicebankOtoEntry&)> handoff)
{
    noteHandoff = std::move(handoff);
    saveButton.setButtonText(utf8("应用到此音符"));
    saveButton.setTooltip(utf8("只作用于这一个音符，不修改 oto 文件"));
    helpLabel.setText(utf8("单独 OTO：只作用于这一个音符，不修改 oto 文件；数值单位均为 ms"),
                      juce::dontSendNotification);
}

OtoWaveformEditorComponent::~OtoWaveformEditorComponent()
{
    // Closing the window is how most playing ends: the device must not be left
    // calling a player that is about to go.
    stopPlayback();
}

void OtoWaveformEditorComponent::togglePlayback()
{
    if (preview != nullptr) stopPlayback();
    else startPlayback();
}

void OtoWaveformEditorComponent::startPlayback()
{
    stopPlayback();
    // The whole recording the entry is cut from, not only 偏移 to 终止: what is
    // either side of the entry is how you hear where its edges belong.
    auto rate = 0.0;
    auto recording = readRecording(edited.audioFile, rate);
    if (recording.getNumSamples() <= 0) return;
    if (playbackHost.beforeStart && !playbackHost.beforeStart()) return;
    juce::AudioDeviceManager* devices = nullptr;
    if (playbackHost.devices)
    {
        devices = playbackHost.devices();
        if (devices == nullptr) return;
    }
    preview = std::make_unique<SourcePreview>(std::move(recording), rate);
    if (devices != nullptr)
    {
        previewPlayer = std::make_unique<juce::AudioSourcePlayer>();
        previewPlayer->setSource(preview.get());
        devices->addAudioCallback(previewPlayer.get());
        previewDevices = devices;
    }
    playButton.setButtonText(utf8("停止播放"));
    waveform.setPlayheadMilliseconds(0.0);
    startTimerHz(30);
}

void OtoWaveformEditorComponent::stopPlayback()
{
    stopTimer();
    if (previewDevices != nullptr)
    {
        // Only while that device is still there to be told: the window can
        // outlive whoever handed it over, and the device along with them.
        if (playbackHost.devices && playbackHost.devices() == previewDevices)
            previewDevices->removeAudioCallback(previewPlayer.get());
        previewDevices = nullptr;
    }
    if (previewPlayer != nullptr) previewPlayer->setSource(nullptr);
    previewPlayer.reset();
    preview.reset();
    waveform.setPlayheadMilliseconds({});
    playButton.setButtonText(utf8("播放原音"));
}

void OtoWaveformEditorComponent::timerCallback()
{
    if (preview == nullptr)
    {
        stopTimer();
        return;
    }
    if (preview->finished())
    {
        stopPlayback();
        return;
    }
    waveform.setPlayheadMilliseconds(preview->seconds() * 1000.0);
}

int OtoWaveformEditorComponent::diagnosticPullPreview(juce::AudioBuffer<float>& out,
                                                      double outputRate, double maxSeconds)
{
    if (preview == nullptr || outputRate <= 0.0) return 0;
    constexpr int block = 512;
    preview->prepareToPlay(block, outputRate);
    const auto wanted = static_cast<int>(std::llround(maxSeconds * outputRate));
    juce::AudioBuffer<float> scratch(2, block);
    auto added = 0;
    while (preview != nullptr && added < wanted)
    {
        const auto count = std::min(block, wanted - added);
        scratch.clear();
        const auto produced = preview->render(scratch, 0, count);
        if (produced > 0)
        {
            const auto base = out.getNumSamples();
            out.setSize(2, base + produced, true, true, false);
            for (int channel = 0; channel < 2; ++channel)
                out.copyFrom(channel, base, scratch, channel, 0, produced);
        }
        added += produced;
        timerCallback();
        if (produced < count) break;
    }
    return added;
}

void OtoWaveformEditorComponent::closeWindow()
{
    if (auto* window = findParentComponentOfClass<juce::DialogWindow>())
        window->closeButtonPressed();
}

juce::String OtoWaveformEditorComponent::formatNumber(double value)
{
    if (std::abs(value) < 0.0005) value = 0.0;
    auto text = juce::String(value, 3);
    while (text.containsChar('.') && text.endsWithChar('0')) text = text.dropLastCharacters(1);
    if (text.endsWithChar('.')) text = text.dropLastCharacters(1);
    return text;
}
}
