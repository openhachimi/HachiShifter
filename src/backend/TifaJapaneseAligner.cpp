#include "TifaJapaneseAligner.h"
#include "AnnotationOrtCancellation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

/* Flat decoder ported from openvpi/TIFA modules/decoding.py, commit
   d246be1cc2e202613213ca60a4e5d35e71a2fb6a.
   MIT License -- Copyright (c) 2026 Team OpenVPI
   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:
   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.
   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE. Model weights have separate licensing; this notice covers code.
*/
namespace hachi::backend
{
namespace
{
constexpr int maxFrames = 6000, maxTokens = 1024;
constexpr std::size_t maxMatrix = 8 * 1024 * 1024;
void checkCancel(const AnnotationCancel& cancel)
{
    if (cancel && cancel()) throw std::runtime_error("Annotation cancelled");
}
using FrameSpan = std::array<int, 2>;
// Preserve float32 arithmetic, update order and strict-greater ties of the
// authoritative _decode_flat_single, including zero-time group-internal skips.
std::vector<FrameSpan> decodeFlat(const float* sim, int T, int N,
    const std::vector<std::int64_t>& groups, const AnnotationCancel& cancel)
{
    if (T < 1 || T > maxFrames || N < 1 || N > maxTokens
        || std::size_t(T + 1) * std::size_t(N + 1) > maxMatrix)
        throw std::runtime_error("TIFA alignment matrix exceeds safety limits");
    std::vector<unsigned char> allowed(std::size_t(N + 1), 1);
    if (!groups.empty()) for (int i = 1; i < N; ++i)
        allowed[std::size_t(i)] = groups[std::size_t(i - 1)] != groups[std::size_t(i)];
    const auto inf = -std::numeric_limits<float>::infinity();
    std::vector<float> gap(std::size_t(N + 1), inf), token(std::size_t(N), inf);
    std::vector<float> nextGap(std::size_t(N + 1), inf), nextToken(std::size_t(N), inf);
    std::vector<unsigned char> gb(std::size_t(T + 1) * std::size_t(N + 1), 0);
    std::vector<unsigned char> tb(std::size_t(T + 1) * std::size_t(N), 0);
    gap[0] = 0.0f;
    for (int i = 0; i < N; ++i) { gap[std::size_t(i + 1)] = gap[std::size_t(i)] - .5f; gb[std::size_t(i + 1)] = 2; }
    for (int t = 1; t <= T; ++t)
    {
        checkCancel(cancel);
        std::fill(nextGap.begin(), nextGap.end(), inf);
        for (int i = 0; i < N; ++i)
        {
            float best = token[std::size_t(i)];
            if (gap[std::size_t(i)] > best)
            { best = gap[std::size_t(i)]; tb[std::size_t(t) * N + i] = 1; }
            const float emission = sim[std::size_t(t - 1) * N + i];
            if (!std::isfinite(emission)) throw std::runtime_error("Non-finite TIFA cosine similarity");
            nextToken[std::size_t(i)] = best + emission;
        }
        for (int i = 0; i <= N; ++i) if (allowed[std::size_t(i)]) nextGap[std::size_t(i)] = gap[std::size_t(i)];
        for (int i = 0; i < N; ++i)
        {
            auto& back = gb[std::size_t(t) * (N + 1) + i + 1];
            if (nextToken[std::size_t(i)] > nextGap[std::size_t(i + 1)])
            { nextGap[std::size_t(i + 1)] = nextToken[std::size_t(i)]; back = 1; }
            const float skipped = nextGap[std::size_t(i)] - .5f;
            if (skipped > nextGap[std::size_t(i + 1)])
            { nextGap[std::size_t(i + 1)] = skipped; back = 2; }
        }
        gap.swap(nextGap); token.swap(nextToken);
    }
    std::vector<FrameSpan> spans(std::size_t(N), FrameSpan{-1, -1});
    int t = T, i = N; bool inToken = false;
    while (t > 0 || i > 0 || inToken)
    {
        checkCancel(cancel);
        if (inToken)
        {
            if (i < 0 || i >= N || t < 1) throw std::runtime_error("Invalid TIFA token backtrace");
            auto& span = spans[std::size_t(i)];
            if (span[1] < 0) span[1] = t;
            span[0] = t - 1;
            const auto entered = tb[std::size_t(t) * N + i];
            --t; if (entered == 1) inToken = false;
        }
        else
        {
            if (t < 0 || i < 0 || i > N) throw std::runtime_error("Invalid TIFA gap backtrace");
            const auto source = gb[std::size_t(t) * (N + 1) + i];
            if (source == 0) { if (t == 0) throw std::runtime_error("Invalid TIFA wait"); --t; }
            else if (source == 1) { --i; inToken = true; }
            else { if (i == 0) throw std::runtime_error("Invalid TIFA skip"); --i; spans[std::size_t(i)] = {t, t}; }
        }
    }
    for (int lo = 0; lo < N;)
    {
        if (spans[std::size_t(lo)][0] != spans[std::size_t(lo)][1]) { ++lo; continue; }
        int hi = lo;
        while (hi + 1 < N && spans[std::size_t(hi + 1)][0] == spans[std::size_t(hi + 1)][1]) ++hi;
        const int left = lo > 0 ? spans[std::size_t(lo - 1)][1] : 0;
        const int right = hi + 1 < N ? spans[std::size_t(hi + 1)][0] : T;
        int gapIndex = lo;
        while (gapIndex <= hi + 1 && !allowed[std::size_t(gapIndex)]) ++gapIndex;
        if (gapIndex > hi + 1 && left != right) throw std::runtime_error("Invalid TIFA grouped skip run");
        for (int j = lo; j <= hi; ++j) { const int anchor = j < gapIndex ? left : right; spans[std::size_t(j)] = {anchor, anchor}; }
        lo = hi + 1;
    }
    return spans;
}
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Port { const char* name; ONNXTensorElementDataType type; std::vector<int64_t> shape; };
void validatePorts(Ort::Session& session, bool inputs, const std::vector<Port>& ports)
{
    const auto count = inputs ? session.GetInputCount() : session.GetOutputCount();
    require(count == ports.size(), "Unexpected TIFA graph port count");
    Ort::AllocatorWithDefaultOptions allocator;
    std::vector<bool> seen(ports.size(), false);
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto name = inputs ? session.GetInputNameAllocated(i, allocator) : session.GetOutputNameAllocated(i, allocator);
        std::size_t p = 0;
        while (p < ports.size() && std::string(name.get()) != ports[p].name) ++p;
        require(p < ports.size() && !seen[p], "Unexpected TIFA graph port name"); seen[p] = true;
        // Tensor shape info is a borrowed view: retain its owning TypeInfo.
        const auto type = inputs ? session.GetInputTypeInfo(i) : session.GetOutputTypeInfo(i);
        require(type.GetONNXType() == ONNX_TYPE_TENSOR, "TIFA graph port is not a tensor");
        const auto info = type.GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        require(info.GetElementType() == ports[p].type && shape.size() == ports[p].shape.size(), "Unexpected TIFA graph port type/rank");
        for (std::size_t d = 0; d < shape.size(); ++d)
            require(shape[d] != 0 && (shape[d] < 0 || ports[p].shape[d] < 0 || shape[d] == ports[p].shape[d]), "Unexpected TIFA graph dimension");
    }
}
void validateTensor(const Ort::Value& value, ONNXTensorElementDataType type, const std::vector<int64_t>& shape)
{
    require(value.IsTensor(), "TIFA output is not a tensor");
    const auto info = value.GetTensorTypeAndShapeInfo();
    require(info.GetElementType() == type && info.GetShape() == shape, "Unexpected TIFA runtime tensor shape/type");
}
std::unique_ptr<Ort::Session> openSession(Ort::Env& env, const juce::File& file, Ort::SessionOptions& options)
{
    const auto path = file.getFullPathName(); // Keep storage alive through Session construction.
   #if JUCE_WINDOWS
    return std::make_unique<Ort::Session>(env, path.toWideCharPointer(), options);
   #else
    return std::make_unique<Ort::Session>(env, path.toRawUTF8(), options);
   #endif
}
#endif
}

bool TifaJapaneseAligner::align(const AnnotationAudio& audio, const juce::StringArray& phones,
    const std::vector<std::int64_t>& groups, const juce::File& directory,
    const OrtExecutionConfig& execution, TifaAlignmentResult& result, juce::String& error,
    const AnnotationCancel& cancel)
{
    result = {}; error.clear();
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
    try
    {
        checkCancel(cancel);
        const int N = phones.size();
        require(N > 0 && N <= maxTokens, "TIFA requires 1 to 1024 known phones");
        require(groups.empty() || groups.size() == std::size_t(N), "TIFA phone/group counts differ");
        require(std::isfinite(audio.duration) && audio.duration > 0 && audio.duration <= 60
            && std::isfinite(audio.sampleRate) && audio.sampleRate >= 8000 && audio.sampleRate <= 384000
            && audio.mono.getNumChannels() == 1 && audio.mono.getNumSamples() > 0
            && std::abs(audio.duration - audio.mono.getNumSamples() / audio.sampleRate) < 1.0 / audio.sampleRate,
            "Invalid immutable annotation audio");
        const std::array<juce::File, 5> files {audio.file, directory.getChildFile("spectrogram.onnx"),
            directory.getChildFile("model.onnx"), directory.getChildFile("config.json"), directory.getChildFile("vocabulary.json")};
        std::array<juce::String, 5> hashes;
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            checkCancel(cancel);
            require(files[i].existsAsFile() && files[i].getSize() > 0, "TIFA resource or recording is missing");
            if (i > 0) require(files[i].getSize() <= 512LL * 1024 * 1024, "TIFA resource exceeds safety limits");
            hashes[i] = juce::SHA256(files[i]).toHexString();
        }
        require(hashes[0] == audio.fingerprint, "Recording changed since annotation decode");
        require(files[3].getSize() <= 65536 && files[4].getSize() <= 1024 * 1024, "TIFA metadata exceeds safety limits");
        const auto config = juce::JSON::parse(files[3].loadFileAsString());
        const auto vocabulary = juce::JSON::parse(files[4].loadFileAsString());
        require(config.isObject() && vocabulary.isObject(), "Invalid TIFA JSON metadata");
        auto number = [&](const char* key, double expected)
        {
            const auto value = config.getProperty(key, juce::var());
            require((value.isInt() || value.isInt64() || value.isDouble()) && std::isfinite(double(value))
                && std::abs(double(value) - expected) < 1.e-10, "Unsupported TIFA configuration");
        };
        number("samplerate", 48000); number("timestep", .01); number("hop_size", 480);
        number("fft_size", 2048); number("win_size", 2048); number("num_mels", 80); number("vocab_size", 256);
        const auto symbols = vocabulary.getProperty("symbols", juce::var());
        require(symbols.isObject(), "TIFA vocabulary has no symbols object");
        for (const auto& property : symbols.getDynamicObject()->getProperties())
            require((property.value.isInt() || property.value.isInt64()) && int64_t(property.value) >= 3
                && int64_t(property.value) < 256, "Invalid TIFA vocabulary ID");
        std::vector<int64_t> tokens; tokens.reserve(std::size_t(N));
        for (const auto& phone : phones)
        {
            require(phone.isNotEmpty(), "Empty TIFA phone");
            auto id = symbols.getProperty(juce::Identifier(phone), juce::var());
            if (id.isVoid()) id = symbols.getProperty(juce::Identifier("ja/" + phone), juce::var());
            if (id.isVoid()) throw std::runtime_error(("Unknown TIFA phone: " + phone).toStdString());
            tokens.push_back(int64_t(id));
        }
        std::vector<float> waveform;
        if (!audio.resample(48000, waveform, error, cancel))
            throw std::runtime_error(error.isNotEmpty() ? error.toStdString() : "Annotation cancelled");
        require(waveform.size() > 784 && waveform.size() <= 48000 * 60, "Recording is too short or long for TIFA spectrogram");
        for (std::size_t i = 0; i < waveform.size(); ++i)
        { if ((i & 8191) == 0) checkCancel(cancel); require(std::isfinite(waveform[i]), "Non-finite annotation waveform"); }
        const int T = int(waveform.size() / 480);
        require(T > 0 && T <= maxFrames && std::size_t(T + 1) * (N + 1) <= maxMatrix, "TIFA matrix exceeds safety limits");
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "HachiTIFA");
        juce::String backend;
        auto options = makeOrtSessionOptions(execution, backend);
        auto spectrogramSession = openSession(env, files[1], options);
        checkCancel(cancel);
        auto modelSession = openSession(env, files[2], options);
        validatePorts(*spectrogramSession, true, {{"waveform", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, -1}}, {"duration", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1}}});
        validatePorts(*spectrogramSession, false, {{"spectrogram", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, -1, 80}}, {"maskT", ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, {1, -1}}});
        validatePorts(*modelSession, true, {{"spectrogram", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, -1, 80}}, {"tokens", ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64, {1, -1}}, {"maskT", ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, {1, -1}}, {"maskN", ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, {1, -1}}});
        validatePorts(*modelSession, false, {{"similarities", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, -1, -1}}, {"logits", ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, -1, 256}}});
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 2> waveShape {1, int64_t(waveform.size())};
        const std::array<int64_t, 1> durationShape {1};
        float duration = float(audio.duration);
        std::array<Ort::Value, 2> specInputs {
            Ort::Value::CreateTensor<float>(memory, waveform.data(), waveform.size(), waveShape.data(), waveShape.size()),
            Ort::Value::CreateTensor<float>(memory, &duration, 1, durationShape.data(), durationShape.size())};
        const char* specNames[] {"waveform", "duration"}; const char* specOutputs[] {"spectrogram", "maskT"};
        std::vector<Ort::Value> features;
        {
            checkCancel(cancel); Ort::RunOptions run;
            AnnotationOrtCancellation watcher(run, cancel);
            features = spectrogramSession->Run(run, specNames, specInputs.data(), specInputs.size(), specOutputs, 2);
        }
        checkCancel(cancel);
        require(features.size() == 2, "Unexpected TIFA spectrogram output count");
        validateTensor(features[0], ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, T, 80});
        validateTensor(features[1], ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL, {1, T});
        const auto* maskT = features[1].GetTensorData<bool>();
        int validFrames = 0; bool ended = false;
        for (int t = 0; t < T; ++t)
        { if (!maskT[t]) ended = true; else { require(!ended, "Non-prefix TIFA frame mask"); ++validFrames; } }
        // ONNX round is ties-to-even; the graph's returned prefix mask is the
        // authority, but verify it against its documented duration convention.
        const double frameDuration = double(duration / .01f);
        const double lower = std::floor(frameDuration), fraction = frameDuration - lower;
        const auto rounded = int(lower + (fraction > .5 || (fraction == .5 && std::fmod(lower, 2.0) != 0) ? 1 : 0));
        require(validFrames > 0 && validFrames == std::min(T, rounded), "Invalid TIFA duration/frame mask");
        const auto* mel = features[0].GetTensorData<float>();
        for (std::size_t i = 0; i < std::size_t(T) * 80; ++i)
        { if ((i & 8191) == 0) checkCancel(cancel); require(std::isfinite(mel[i]), "Non-finite TIFA spectrogram"); }
        const std::array<int64_t, 2> tokenShape {1, N};
        auto maskN = std::make_unique<bool[]>(std::size_t(N)); std::fill_n(maskN.get(), N, true);
        std::array<Ort::Value, 4> modelInputs {std::move(features[0]),
            Ort::Value::CreateTensor<int64_t>(memory, tokens.data(), tokens.size(), tokenShape.data(), 2), std::move(features[1]),
            Ort::Value::CreateTensor<bool>(memory, maskN.get(), std::size_t(N), tokenShape.data(), 2)};
        const char* modelNames[] {"spectrogram", "tokens", "maskT", "maskN"}; const char* modelOutputs[] {"similarities"};
        std::vector<Ort::Value> outputs;
        {
            checkCancel(cancel); Ort::RunOptions run;
            AnnotationOrtCancellation watcher(run, cancel);
            outputs = modelSession->Run(run, modelNames, modelInputs.data(), modelInputs.size(), modelOutputs, 1);
        }
        checkCancel(cancel);
        require(outputs.size() == 1, "Unexpected TIFA similarity output count");
        validateTensor(outputs[0], ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, {1, T, N});
        const auto* sim = outputs[0].GetTensorData<float>();
        const auto spans = decodeFlat(sim, validFrames, N, groups, cancel);
        TifaAlignmentResult candidate; candidate.activeBackend = backend; candidate.timestep = .01;
        candidate.modelFingerprint = juce::SHA256((hashes[1] + hashes[2] + hashes[3] + hashes[4]).toRawUTF8(), std::size_t(256)).toHexString();
        const double end = std::min(audio.duration, validFrames * candidate.timestep);
        for (int i = 0; i < N; ++i)
        {
            checkCancel(cancel); const auto& span = spans[std::size_t(i)];
            require(span[0] >= 0 && span[1] >= span[0] && span[1] <= validFrames, "Invalid decoded TIFA span");
            double mean = 0;
            for (int t = span[0]; t < span[1]; ++t) mean += sim[std::size_t(t) * N + i];
            if (span[0] == span[1]) ++candidate.skippedPhones;
            else mean /= span[1] - span[0]; // Raw cosine agreement, NOT probability.
            candidate.phones.push_back({phones[i], std::min(end, span[0] * candidate.timestep),
                std::min(end, span[1] * candidate.timestep), mean});
        }
        for (std::size_t i = 0; i < files.size(); ++i)
        { checkCancel(cancel); require(files[i].existsAsFile() && juce::SHA256(files[i]).toHexString() == hashes[i], "Recording or TIFA resources changed during alignment"); }
        checkCancel(cancel); result = std::move(candidate); return true;
    }
    catch (const Ort::Exception& e) { error = cancel && cancel() ? "Annotation cancelled" : "TIFA ONNX failure: " + juce::String(e.what()); }
    catch (const std::exception& e) { error = cancel && cancel() ? "Annotation cancelled" : juce::String(e.what()); }
    catch (...) { error = "Unknown TIFA alignment failure"; }
#else
    juce::ignoreUnused(audio, phones, groups, directory, execution, cancel);
    error = "TIFA requires the native ONNX analysis build";
#endif
    return false;
}
}
