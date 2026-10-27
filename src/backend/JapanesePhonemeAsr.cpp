#include "JapanesePhonemeAsr.h"
#include "AnnotationOrtCancellation.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>

namespace hachi::backend
{
bool JapanesePhonemeAsr::transcribe(const AnnotationAudio& audio, const juce::File& directory,
    const OrtExecutionConfig& execution, JapaneseAsrResult& result, juce::String& error,
    const AnnotationCancel& cancel)
{
    error.clear();
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
    const auto model = directory.getChildFile("model.onnx");
    const auto vocab = directory.getChildFile("phoneme_vocab.json");
    const auto metadata = directory.getChildFile("model.meta.json");
    if (!model.existsAsFile() || !vocab.existsAsFile() || !metadata.existsAsFile())
    { error = "Japanese ASR pack is incomplete: model.onnx, phoneme_vocab.json and model.meta.json are required"; return false; }
    const auto config = juce::JSON::parse(metadata);
    if (int(config["sample_rate"]) != 16000 || int(config["blank_id"]) != 0
        || config["export_mode"].toString() != "minimal_pred_ids")
    { error = "Unsupported Japanese ASR metadata; this adapter requires the 16 kHz phoneme pred_ids model"; return false; }
    const auto vocabulary = juce::JSON::parse(vocab);
    std::map<std::int64_t, juce::String> symbols;
    if (const auto* object = vocabulary.getDynamicObject())
        for (const auto& property : object->getProperties())
        {
            const auto id = std::int64_t(property.value);
            if (id < 0 || symbols.contains(id)) { error = "Malformed Japanese ASR vocabulary"; return false; }
            symbols.emplace(id, property.name.toString());
        }
    if (!symbols.contains(0) || symbols.at(0) != "<blank>" || symbols.size() < 5)
    { error = "Japanese ASR vocabulary has no valid blank token"; return false; }
    std::vector<float> input;
    if (!audio.resample(16000, input, error, cancel)) return false;
    if (cancel && cancel()) { error = "Annotation cancelled"; return false; }
    const auto modelHash = juce::SHA256(model).toHexString();
    const auto vocabHash = juce::SHA256(vocab).toHexString();
    const auto metadataHash = juce::SHA256(metadata).toHexString();
    try
    {
        static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "hachi-japanese-asr");
        JapaneseAsrResult decoded;
        auto options = makeOrtSessionOptions(execution, decoded.activeBackend);
        const auto path = model.getFullPathName();
        Ort::Session session(environment, path.toWideCharPointer(), options);
        if (session.GetInputCount() != 1 || session.GetOutputCount() != 1)
        { error = "Unexpected Japanese ASR tensor count"; return false; }
        const auto inputInfo = session.GetInputTypeInfo(0);
        const auto outputInfo = session.GetOutputTypeInfo(0);
        const auto inputType = inputInfo.GetTensorTypeAndShapeInfo();
        const auto outputType = outputInfo.GetTensorTypeAndShapeInfo();
        if (inputType.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT
            || inputType.GetShape().size() != 2
            || outputType.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64
            || outputType.GetShape().size() != 2)
        { error = "Unexpected Japanese ASR tensor types or ranks"; return false; }
        Ort::AllocatorWithDefaultOptions allocator;
        const auto inputName = session.GetInputNameAllocated(0, allocator);
        const auto outputName = session.GetOutputNameAllocated(0, allocator);
        if (juce::String(inputName.get()) != "input_values" || juce::String(outputName.get()) != "pred_ids")
        { error = "Japanese ASR tensor names do not match the deployment contract"; return false; }
        const std::int64_t shape[]{1, std::int64_t(input.size())};
        const auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto tensor = Ort::Value::CreateTensor<float>(memory, input.data(), input.size(), shape, 2);
        const char* inputs[]{"input_values"}; const char* outputs[]{"pred_ids"};
        Ort::RunOptions runOptions;
        std::vector<Ort::Value> values;
        {
            AnnotationOrtCancellation interruption(runOptions, cancel);
            values = session.Run(runOptions, inputs, &tensor, 1, outputs, 1);
        }
        if (cancel && cancel()) { error = "Annotation cancelled"; return false; }
        if (values.size() != 1 || !values[0].IsTensor())
        { error = "Japanese ASR returned no prediction tensor"; return false; }
        const auto info = values[0].GetTensorTypeAndShapeInfo();
        const auto dimensions = info.GetShape();
        if (dimensions.size() != 2 || dimensions[0] != 1 || dimensions[1] < 1 || dimensions[1] > 100000)
        { error = "Invalid Japanese ASR prediction shape"; return false; }
        decoded.frames = std::size_t(dimensions[1]);
        const auto* ids = values[0].GetTensorData<std::int64_t>();
        std::int64_t previous = -1;
        for (std::size_t frame = 0; frame < decoded.frames; ++frame)
        {
            const auto id = ids[frame];
            if (!symbols.contains(id)) { error = "Japanese ASR returned an unknown vocabulary ID"; return false; }
            if (id != previous && id != 0)
            {
                const auto token = symbols.at(id);
                if (token == "<unk>")
                { error = "Japanese ASR predicted an unknown phone; review/correct the sequence before alignment"; return false; }
                if (token != "SOS" && token != "EOS") decoded.tokens.add(token);
            }
            previous = id;
        }
        if (decoded.tokens.isEmpty())
        { error = "No Japanese phonemes were recognized; no annotation was generated"; return false; }
        if (modelHash != juce::SHA256(model).toHexString() || vocabHash != juce::SHA256(vocab).toHexString()
            || metadataHash != juce::SHA256(metadata).toHexString())
        { error = "Japanese ASR model pack changed during recognition"; return false; }
        decoded.modelFingerprint = modelHash + ":" + vocabHash + ":" + metadataHash;
        result = std::move(decoded); return true;
    }
    catch (const Ort::Exception& exception)
    {
        error = cancel && cancel() ? juce::String("Annotation cancelled")
            : "Native Japanese ASR inference failed: " + juce::String::fromUTF8(exception.what());
    }
    catch (const std::exception& exception)
    { error = "Japanese ASR failed: " + juce::String::fromUTF8(exception.what()); }
    return false;
#else
    juce::ignoreUnused(audio, directory, execution, result, cancel);
    error = "This build has no native ONNX annotation runtime"; return false;
#endif
}
}
