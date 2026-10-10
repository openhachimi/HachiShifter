#include "NativePitchVoicingDisplay.h"
#include "backend/SourceBreathiness.h"
#include "NativePitchIdentity.h"
#include <atomic>
#include <map>
#include <set>

namespace hachi
{
struct NativePitchVoicingCache::State
{
    struct Entry
    {
        juce::String stamp;
        std::atomic<bool> active { true };
        std::shared_ptr<const NativeNoiseRanges> ranges;
    };
    juce::CriticalSection lock;
    std::map<juce::String, std::shared_ptr<Entry>> files;
};

struct NativePitchVoicingCache::Job final : juce::ThreadPoolJob
{
    Job(NativePitchVoicingCache& cache, juce::File fileToRead, std::shared_ptr<State::Entry> entryToFill)
        : ThreadPoolJob("Native breath display"), owner(cache), file(std::move(fileToRead)),
          entry(std::move(entryToFill)) {}
    JobStatus runJob() override
    {
        const auto cancelled = [&] { return shouldExit() || !entry->active.load(); };
        auto ranges = std::make_shared<NativeNoiseRanges>();
        try
        {
            juce::AudioFormatManager formats; formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
            if (reader && reader->sampleRate > 0 && reader->numChannels > 0)
            {
                // Stream into a 16 kHz mono buffer rather than keeping a full
                // multichannel recording in memory. The same linear resampling
                // clock is used by the renderer. Breath evidence is visual;
                // pitched breath is deliberately broader than the render UV mask.
                constexpr double rate = 16000;
                const auto count = static_cast<std::size_t>(std::ceil(reader->lengthInSamples * rate / reader->sampleRate));
                std::vector<float> mono(count);
                constexpr int chunk = 16384;
                juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), chunk + 1);
                std::size_t output = 0;
                for (juce::int64 start = 0; start < reader->lengthInSamples; start += chunk)
                {
                    if (cancelled()) return jobHasFinished;
                    const auto n = static_cast<int>(std::min<juce::int64>(chunk + 1, reader->lengthInSamples - start));
                    buffer.clear();
                    if (!reader->read(&buffer, 0, n, start, true, true)) break;
                    const auto end = std::min(start + chunk, reader->lengthInSamples);
                    while (output < count && output * reader->sampleRate / rate < end)
                    {
                        const auto pos = std::min(double(reader->lengthInSamples - 1), output * reader->sampleRate / rate) - start;
                        const auto a = juce::jlimit(0, n - 1, static_cast<int>(pos));
                        const auto b = std::min(a + 1, n - 1);
                        float sample = 0;
                        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                            sample += buffer.getSample(channel, a) + (buffer.getSample(channel, b) - buffer.getSample(channel, a)) * float(pos - a);
                        mono[output++] = sample / float(buffer.getNumChannels());
                    }
                }
                mono.resize(output);
                const auto mask = backend::SourceBreathiness::analyse(mono, rate,
                    [&] { if (cancelled()) throw 0; });
                for (std::size_t i = 1; i < mask.marked.size(); ++i)
                    if (mask.marked[i - 1] >= .999f && mask.marked[i] >= .999f)
                        ranges->emplace_back((i - 1) * mask.period, i * mask.period);
                mergeNativeNoiseRanges(*ranges);
            }
        }
        catch (...) { if (cancelled()) return jobHasFinished; }
        if (cancelled()) return jobHasFinished;
        {
            const juce::ScopedLock guard(owner.state->lock);
            entry->ranges = std::move(ranges);
        }
        owner.sendChangeMessage();
        return jobHasFinished;
    }
    NativePitchVoicingCache& owner;
    juce::File file;
    std::shared_ptr<State::Entry> entry;
};

NativePitchVoicingCache::NativePitchVoicingCache() : state(std::make_shared<State>()) {}
NativePitchVoicingCache::~NativePitchVoicingCache()
{
    pool.removeAllJobs(true, -1); // Cancellation is checked at every read chunk / analysis frame.
}

void NativePitchVoicingCache::request(const std::vector<juce::File>& files)
{
    std::set<juce::String> wanted;
    const juce::ScopedLock guard(state->lock);
    for (const auto& file : files)
    {
        const auto key = file.getFullPathName();
        if (!file.existsAsFile() || !wanted.insert(key).second) continue;
        const auto stamp = juce::String(file.getSize()) + ":" + juce::String(file.getLastModificationTime().toMilliseconds());
        const auto found = state->files.find(key);
        if (found != state->files.end() && found->second->stamp == stamp) continue;
        if (found != state->files.end()) found->second->active = false;
        auto entry = std::make_shared<State::Entry>(); entry->stamp = stamp;
        state->files[key] = entry;
        pool.addJob(new Job(*this, file, entry), true);
    }
    for (auto i = state->files.begin(); i != state->files.end();)
        if (!wanted.contains(i->first)) { i->second->active = false; i = state->files.erase(i); }
        else ++i;
}

std::shared_ptr<const NativeNoiseRanges> NativePitchVoicingCache::rangesFor(const juce::File& file) const
{
    const juce::ScopedLock guard(state->lock);
    const auto found = state->files.find(file.getFullPathName());
    return found == state->files.end() ? nullptr : found->second->ranges;
}
bool NativePitchVoicingCache::pending() const
{
    const juce::ScopedLock guard(state->lock);
    for (const auto& [_, entry] : state->files) if (!entry->ranges) return true;
    return false;
}

void mergeNativeNoiseRanges(NativeNoiseRanges& ranges)
{
    std::sort(ranges.begin(), ranges.end());
    std::size_t count = 0;
    for (const auto& span : ranges)
    {
        if (span.second <= span.first + 1.e-9) continue;
        if (count && ranges[count - 1].second >= span.first - 1.e-9)
            ranges[count - 1].second = std::max(ranges[count - 1].second, span.second);
        else ranges[count++] = span;
    }
    ranges.resize(count);
}

NativeNoiseRanges nativeNoiseDisplayRanges(const ClipData& clip, const NoteData& note,
                                          const NativeNoiseRanges& sourceRanges)
{
    const auto audio = nativeAudioPreviewClip(clip);
    const auto map = nativeSourceTimeMap(audio);
    NativeNoiseRanges result;
    if (map.empty() || audio.durationSeconds <= 0) return result;
    const auto sourceFrom = audio.sourceOffsetSeconds + map.front().sourceSeconds;
    const auto sourceTo = audio.sourceOffsetSeconds + map.back().sourceSeconds;
    const auto noteFrom = clip.startSeconds + note.startSeconds;
    const auto noteTo = noteFrom + note.durationSeconds;
    for (const auto& span : sourceRanges)
    {
        if (span.second <= sourceFrom) continue;
        if (span.first >= sourceTo) break;
        const auto from = std::max(noteFrom, audio.startSeconds + nativeTargetTimeAt(map,
            std::max(span.first, sourceFrom) - audio.sourceOffsetSeconds));
        const auto to = std::min(noteTo, audio.startSeconds + nativeTargetTimeAt(map,
            std::min(span.second, sourceTo) - audio.sourceOffsetSeconds));
        if (to > from + 1.e-9) result.emplace_back(from, to);
    }
    return result;
}

NativeNoiseRanges measuredUnvoicedDisplayRanges(const ClipData& clip, const NoteData& note)
{
    NativeNoiseRanges result;
    if (!note.sourcePitchMeasured) return result;
    for (std::size_t i = 1; i < note.contour.size(); ++i)
        if (!note.contour[i - 1].voiced && !note.contour[i].voiced)
        {
            const auto from = std::max(0.0, note.contour[i - 1].timeSeconds);
            const auto to = std::min(note.durationSeconds, note.contour[i].timeSeconds);
            if (to > from) result.emplace_back(clip.startSeconds + note.startSeconds + from,
                                             clip.startSeconds + note.startSeconds + to);
        }
    mergeNativeNoiseRanges(result);
    return result;
}

NativeNoiseRanges nativePitchDisplayRanges(const ClipData& clip, const NoteData& note)
{
    NativeNoiseRanges result;
    if (note.nativeUnpitched || nativePendingPitchIsNeutral(note)) return result;
    // Match contour interpolation: both bounding samples must be voiced.
    // In particular, don't hold the first/last pitch into an unmeasured tail.
    for (std::size_t i = 1; i < note.contour.size(); ++i)
        if (note.contour[i - 1].voiced && note.contour[i].voiced)
        {
            const auto from = std::max(clip.audioStartSeconds,
                note.startSeconds + std::max(0.0, note.contour[i - 1].timeSeconds));
            const auto to = std::min(clip.audioStartSeconds + clip.audioLength(),
                note.startSeconds + std::min(note.durationSeconds, note.contour[i].timeSeconds));
            if (to > from) result.emplace_back(clip.startSeconds + from, clip.startSeconds + to);
        }
    mergeNativeNoiseRanges(result);
    return result;
}

bool reduceNativePitchClip(juce::Graphics& g, const NativeNoiseRanges& ranges,
                          float pixelsPerSecond, float timeZeroX)
{
    juce::RectangleList<int> voiced;
    const auto clip = g.getClipBounds();
    for (const auto& span : ranges)
    {
        const auto from = timeZeroX + float(span.first) * pixelsPerSecond;
        const auto to = timeZeroX + float(span.second) * pixelsPerSecond;
        voiced.add(juce::Rectangle<int>::leftTopRightBottom(int(std::floor(from)), clip.getY(),
            int(std::ceil(to)), clip.getBottom()).getIntersection(clip));
    }
    return g.reduceClipRegion(voiced);
}

NativeNoiseRanges nativeAuthoredPitchDisplayRanges(const ClipData& clip, const NoteData& note)
{
    NativeNoiseRanges result;
    if (note.nativeUnpitched) return result;
    const auto start = clip.startSeconds + note.startSeconds;
    if (note.pitchControlPoints.size() >= 2)
        result.emplace_back(start + note.pitchControlPoints.front().timeSeconds,
                            start + note.pitchControlPoints.back().timeSeconds);
    else
        for (std::size_t i = 1; i < note.contour.size(); ++i)
            if (note.contour[i - 1].hasManualTarget && note.contour[i].hasManualTarget)
                result.emplace_back(start + note.contour[i - 1].timeSeconds,
                                    start + note.contour[i].timeSeconds);
    mergeNativeNoiseRanges(result);
    return result;
}

void strokeNativeAuthoredPitchDots(juce::Graphics& g, const juce::Path& path,
    const NativeNoiseRanges& voicedRanges, const NativeNoiseRanges& authoredRanges,
    float pixelsPerSecond, float timeZeroX)
{
    if (authoredRanges.empty() || path.isEmpty()) return;
    juce::Graphics::ScopedSaveState saved(g);
    if (!reduceNativePitchClip(g, authoredRanges, pixelsPerSecond, timeZeroX)) return;
    const auto clip = g.getClipBounds();
    juce::RectangleList<int> guide(clip), voiced;
    for (const auto& span : voicedRanges)
    {
        const auto from = timeZeroX + float(span.first) * pixelsPerSecond;
        const auto to = timeZeroX + float(span.second) * pixelsPerSecond;
        voiced.add(juce::Rectangle<int>::leftTopRightBottom(int(std::floor(from)), clip.getY(),
            int(std::ceil(to)), clip.getBottom()).getIntersection(clip));
    }
    guide.subtract(voiced);
    if (!g.reduceClipRegion(guide)) return;
    constexpr float spacing = 6.0f, diameter = 2.2f;
    juce::PathFlatteningIterator segment(path);
    auto nextDot = 0.0f;
    while (segment.next())
    {
        if (segment.subPathIndex == 0) nextDot = 0.0f;
        const auto dx = segment.x2 - segment.x1, dy = segment.y2 - segment.y1;
        const auto length = std::hypot(dx, dy);
        if (length < 1.e-6f) continue;
        auto distance = nextDot;
        // Long clips at high zoom must not emit thousands of offscreen dots.
        if (dx > 1.e-6f)
        {
            const auto enter = (clip.getX() - diameter - segment.x1) * length / dx;
            if (enter > distance) distance += std::ceil((enter - distance) / spacing) * spacing;
        }
        for (; distance <= length; distance += spacing)
        {
            const auto u = distance / length;
            const auto x = segment.x1 + dx * u, y = segment.y1 + dy * u;
            if (dx > 0 && x > clip.getRight() + diameter) break;
            if (x >= clip.getX() - diameter && x <= clip.getRight() + diameter
                && y >= clip.getY() - diameter && y <= clip.getBottom() + diameter)
                g.fillEllipse(x - diameter * .5f, y - diameter * .5f, diameter, diameter);
        }
        // Keep spacing phase along the complete curve, even outside the mask.
        nextDot = std::fmod(nextDot - length, spacing);
        if (nextDot < 0) nextDot += spacing;
    }
}

void strokeNativePitchWithNoise(juce::Graphics& g, const juce::Path& path,
    const NativeNoiseRanges& ranges, float pixelsPerSecond, float timeZeroX, bool dashedOnly)
{
    const juce::PathStrokeType stroke(2.0f, juce::PathStrokeType::curved);
    juce::RectangleList<int> noise;
    const auto clip = g.getClipBounds();
    for (const auto& span : ranges)
    {
        const auto from = timeZeroX + float(span.first) * pixelsPerSecond;
        const auto to = timeZeroX + float(span.second) * pixelsPerSecond;
        noise.add(juce::Rectangle<int>::leftTopRightBottom(int(std::floor(from)), clip.getY(),
            int(std::ceil(to)), clip.getBottom()).getIntersection(clip));
    }
    if (!dashedOnly)
    {
        juce::Graphics::ScopedSaveState saved(g);
        juce::RectangleList<int> solid(clip); solid.subtract(noise);
        g.reduceClipRegion(solid); g.strokePath(path, stroke);
    }
    if (!noise.isEmpty())
    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(noise);
        const float dash[] { 5.0f, 4.0f };
        juce::Path dashed; stroke.createDashedStroke(dashed, path, dash, 2);
        g.fillPath(dashed);
    }
}
}
