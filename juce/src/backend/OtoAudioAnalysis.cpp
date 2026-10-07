#include "OtoAudioAnalysis.h"
#include "AudioFileReader.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <deque>
#include <stdexcept>

namespace hachi::backend
{
double OtoAudioAnalysisData::fractionForHz(double hz) const
{
    return std::log(std::max(minimumHz, hz) / minimumHz) / std::log(maximumHz / minimumHz);
}
double OtoAudioAnalysisData::hzForFraction(double fraction) const
{
    return minimumHz * std::pow(maximumHz / minimumHz, fraction);
}
std::shared_ptr<const OtoAudioAnalysisData> OtoAudioAnalysisRequest::snapshot()
{
    std::scoped_lock lock(mutex); return data;
}
AnalysisConfig OtoAudioAnalysis::editorConfig()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "HachiShifterNext";
    options.filenameSuffix = "settings";
    options.folderName = juce::SystemStats::getEnvironmentVariable(
        "HACHI_TEST_SETTINGS_DIR", "HachiShifterNext");
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    options.millisecondsBeforeSaving = -1;
    juce::PropertiesFile properties(options);
    return AnalysisService::configFromProperties(&properties);
}
namespace
{
struct CacheItem
{
    juce::String key;
    std::shared_ptr<const OtoAudioAnalysisData> data;
};
struct Service : juce::DeletedAtShutdown
{
    ~Service() override { pool.removeAllJobs(true, -1); }
    std::mutex mutex;
    std::deque<CacheItem> cache;
    juce::ThreadPool pool { 1 }; // bound CPU and memory when switching aliases quickly
};
Service& service() { static auto* value = new Service(); return *value; }
juce::String fileKey(const juce::File& file)
{
    return file.getFullPathName() + ":" + juce::String(file.getSize()) + ":"
        + juce::String(file.getLastModificationTime().toMilliseconds());
}
juce::Colour heatColour(float value)
{
    const std::array<juce::Colour, 5> stops {
        juce::Colour(0xff101922), juce::Colour(0xff26334e), juce::Colour(0xff78466f),
        juce::Colour(0xffe68c52), juce::Colour(0xffffe7a3)
    };
    const auto position = juce::jlimit(0.0f, 1.0f, value) * 4.0f;
    const auto index = std::min(3, static_cast<int>(position));
    return stops[static_cast<std::size_t>(index)].interpolatedWith(
        stops[static_cast<std::size_t>(index + 1)], position - index);
}
bool makeSpectrum(const juce::File& file, OtoAudioAnalysisData& data,
                  const std::function<bool()>& cancelled)
{
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    auto reader = createAudioReader(formats, file);
    if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
    { data.error = juce::String::fromUTF8("无法读取原始录音"); return false; }
    const auto rate = reader->sampleRate;
    data.durationSeconds = reader->lengthInSamples / rate;
    data.maximumHz = std::min(12000.0, rate * 0.5);
    data.minimumHz = std::min(40.0, data.maximumHz * 0.25);
    const auto columns = juce::jlimit(1, 8192,
        static_cast<int>(std::min(8192.0, std::ceil(data.durationSeconds / 0.01))));
    constexpr int rows = 256;
    int order = 10;
    while ((1 << order) < rate * 0.04 && order < 13) ++order;
    const int size = 1 << order, bins = size / 2 + 1;
    juce::dsp::FFT fft(order);
    std::vector<std::complex<float>> input(size), output(size);
    std::vector<float> window(size), magnitude(bins), db(static_cast<std::size_t>(columns * rows));
    float windowSum = 0, peakDb = -120;
    for (int i = 0; i < size; ++i)
    {
        window[i] = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * i / (size - 1));
        windowSum += window[i];
    }
    const int channels = juce::jlimit(1, 2, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> buffer(channels, size);
    for (int column = 0; column < columns; ++column)
    {
        if (cancelled()) return false;
        const auto centre = static_cast<juce::int64>((column + 0.5) * reader->lengthInSamples / columns);
        const auto begin = centre - size / 2;
        const auto from = std::max<juce::int64>(0, begin);
        const auto dest = static_cast<int>(std::max<juce::int64>(0, -begin));
        const auto count = static_cast<int>(std::min<juce::int64>(size - dest, reader->lengthInSamples - from));
        buffer.clear();
        if (count > 0 && !reader->read(&buffer, dest, count, from, true, channels > 1))
        { data.error = juce::String::fromUTF8("读取频谱数据失败"); return false; }
        for (int i = 0; i < size; ++i)
        {
            float mono = 0;
            for (int channel = 0; channel < channels; ++channel) mono += buffer.getSample(channel, i);
            input[i] = { mono / channels * window[i], 0.0f };
        }
        fft.perform(input.data(), output.data(), false);
        for (int i = 0; i < bins; ++i) magnitude[i] = std::abs(output[i]) * 2.0f / windowSum;
        for (int row = 0; row < rows; ++row)
        {
            const auto lower = data.hzForFraction(1.0 - (row + 1.0) / rows) * size / rate;
            const auto upper = data.hzForFraction(1.0 - static_cast<double>(row) / rows) * size / rate;
            const auto centreBin = (lower + upper) * 0.5;
            const auto left = juce::jlimit(0, bins - 1, static_cast<int>(centreBin));
            const auto right = std::min(bins - 1, left + 1);
            auto value = magnitude[left] + static_cast<float>(centreBin - left)
                * (magnitude[right] - magnitude[left]);
            for (int bin = std::max(0, static_cast<int>(std::ceil(lower)));
                 bin <= std::min(bins - 1, static_cast<int>(std::floor(upper))); ++bin)
                value = std::max(value, magnitude[bin]);
            const auto level = 20.0f * std::log10(std::max(1.0e-6f, value));
            db[static_cast<std::size_t>(column * rows + row)] = level;
            peakDb = std::max(peakDb, level);
        }
    }
    data.spectrogram = juce::Image(juce::Image::RGB, columns, rows, false, juce::SoftwareImageType());
    juce::Image::BitmapData pixels(data.spectrogram, juce::Image::BitmapData::writeOnly);
    const auto top = juce::jlimit(-45.0f, 0.0f, peakDb);
    for (int column = 0; column < columns; ++column)
    {
        if (cancelled()) return false;
        for (int row = 0; row < rows; ++row)
            pixels.setPixelColour(column, row, heatColour(
                (db[static_cast<std::size_t>(column * rows + row)] - top + 80.0f) / 80.0f));
    }
    return true;
}
}
std::shared_ptr<OtoAudioAnalysisRequest> OtoAudioAnalysis::request(
    const juce::File& audio, const AnalysisConfig& config)
{
    auto state = std::make_shared<OtoAudioAnalysisRequest>();
    const auto status = AnalysisService::status(config);
    const auto model = status.fcpeModelPath;
    const auto key = fileKey(audio) + "|" + fileKey(model) + "|"
        + inferenceBackendName(config.inference) + "|" + juce::String(config.deviceIndex);
    auto& shared = service();
    {
        std::scoped_lock lock(shared.mutex);
        for (const auto& item : shared.cache)
            if (item.key == key) { state->data = item.data; return state; }
    }
    std::weak_ptr<OtoAudioAnalysisRequest> weak = state;
    shared.pool.addJob([weak, audio, model, config, key]
    {
        const auto cancelled = [weak]
        { const auto request = weak.lock(); return !request || request->cancelled.load(); };
        const auto publish = [weak](std::shared_ptr<const OtoAudioAnalysisData> data)
        {
            if (const auto request = weak.lock(); request && !request->cancelled.load())
            { std::scoped_lock lock(request->mutex); request->data = std::move(data); }
        };
        if (cancelled()) return;
        auto result = std::make_shared<OtoAudioAnalysisData>();
        try
        {
            if (makeSpectrum(audio, *result, cancelled))
            {
                publish(std::make_shared<OtoAudioAnalysisData>(*result));
                if (cancelled()) return;
                juce::String pitchError;
                const auto progress = [cancelled](double)
                { if (cancelled()) throw std::runtime_error("OTO analysis cancelled"); };
                if (model.existsAsFile())
                {
                    result->pitch = FcpeAnalyzer::analyse(audio, model,
                        { config.inference, config.deviceIndex, std::min(4, std::max(1, juce::SystemStats::getNumCpus())) },
                        pitchError, progress);
                    if (!result->pitch.empty()) result->pitchBackend = "FCPE";
                }
                if (cancelled()) return;
                if (result->pitch.empty())
                {
                    result->warning = model.existsAsFile() ? pitchError
                        : juce::String::fromUTF8("FCPE 模型不可用，使用内置 F0 识别");
                    juce::String nativeError;
                    const auto notes = NativeAnalyzer::analyse(audio, nativeError, progress);
                    for (const auto& note : notes) for (const auto& point : note.contour)
                    {
                        const double time = note.startSeconds + point.timeSeconds;
                        if (!result->pitch.empty() && time <= result->pitch.back().timeSeconds) continue;
                        result->pitch.push_back({ time, note.sourceMidiCenter + point.relativeCents / 100.0f,
                                                  0.0f, point.voiced });
                    }
                    result->pitchBackend = "native-hq";
                    if (result->pitch.empty()) result->warning += " " + nativeError;
                }
            }
        }
        catch (const std::exception& error) { result->error = juce::String::fromUTF8(error.what()); }
        if (cancelled()) return;
        result->complete = true;
        // Only cache successful FCPE results. A temporary inference failure
        // must be retried when the sample is reopened.
        if (result->error.isEmpty() && result->warning.isEmpty())
        {
            auto& cache = service(); std::scoped_lock lock(cache.mutex);
            cache.cache.push_front({ key, result });
            while (cache.cache.size() > 6) cache.cache.pop_back();
        }
        publish(result);
    });
    return state;
}
}
