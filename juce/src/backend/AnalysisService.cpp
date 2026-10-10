#include "AnalysisService.h"
#include "GameAnalyzer.h"
#include "../NativeSourceTimeMap.h"
#include "../NativeTrimSource.h"
#include "../ClipParts.h"
#include <algorithm>
#include <array>
#include <map>
#include <optional>

namespace hachi::backend
{
namespace
{
constexpr std::array<const char*, 5> gameFiles {
    "encoder.onnx", "segmenter.onnx", "estimator.onnx", "bd2dur.onnx", "config.json"
};

bool isGameDirectory(const juce::File& directory)
{
    if (!directory.isDirectory()) return false;
    return std::all_of(gameFiles.begin(), gameFiles.end(), [&directory](const char* name)
    {
        return directory.getChildFile(name).existsAsFile();
    });
}

juce::File executableDirectory()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
        .getParentDirectory();
}

juce::File environmentFile(const char* name)
{
    const auto value = juce::SystemStats::getEnvironmentVariable(name, {}).trim()
        .unquoted();
    return value.isEmpty() ? juce::File{} : juce::File(value);
}

InferenceBackend inferenceFromId(int id)
{
    if (id == 2) return InferenceBackend::cpu;
    if (id == 3) return InferenceBackend::directML;
    if (id == 4) return InferenceBackend::cuda;
    if (id == 5) return InferenceBackend::coreML;
    return InferenceBackend::automatic;
}

juce::String normalizedGameModel(const juce::String& value)
{
    const auto variant = value.trim().toLowerCase();
    return variant == "small" || variant == "large" ? variant : juce::String("medium");
}

std::optional<std::pair<float, float>> absolutePitchAt(
    const std::vector<NoteData>& notes, double sourceSeconds)
{
    const auto nextNote = std::upper_bound(notes.begin(), notes.end(), sourceSeconds + 1.0e-6,
        [](double t, const NoteData& note) { return t < note.startSeconds; });
    if (nextNote == notes.begin()) return std::nullopt;
    const auto& note = *(nextNote - 1);
    const auto local = sourceSeconds - note.startSeconds;
    if (local < -1.0e-6 || local > note.durationSeconds + 1.0e-6
        || note.contour.empty())
        return std::nullopt;
    const auto right = std::lower_bound(note.contour.begin(), note.contour.end(), local,
        [](const PitchPoint& point, double time) { return point.timeSeconds < time; });
    const auto rightIndex = right == note.contour.end() ? note.contour.size() - 1
        : static_cast<std::size_t>(std::distance(note.contour.begin(), right));
    const auto leftIndex = rightIndex > 0 && note.contour[rightIndex].timeSeconds > local
        ? rightIndex - 1 : rightIndex;
    const auto& left = note.contour[leftIndex];
    const auto& next = note.contour[rightIndex];
    if (!left.voiced || !next.voiced) return std::nullopt;
    const auto amount = next.timeSeconds > left.timeSeconds
        ? static_cast<float>(juce::jlimit(0.0, 1.0,
            (local - left.timeSeconds) / (next.timeSeconds - left.timeSeconds))) : 0.0f;
    const auto interpolate = [amount](float first, float second)
    {
        return first + (second - first) * amount;
    };
    const auto centre = note.sourceMidiCenter * 100.0f;
    return std::pair { centre + interpolate(left.relativeCents, next.relativeCents),
                       centre + interpolate(left.withoutVibratoCents,
                                            next.withoutVibratoCents) };
}

float median(std::vector<float> values)
{
    if (values.empty()) return 6'000.0f;
    const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}
}

AnalysisConfig AnalysisService::configFromProperties(const juce::PropertiesFile* properties)
{
    if (properties == nullptr) return configFromEnvironment();
    AnalysisConfig config;
    const auto game = properties->getValue("algorithm.gamePath").trim();
    const auto fcpe = properties->getValue("algorithm.fcpePath").trim();
    if (game.isNotEmpty()) config.gameModelDirectory = juce::File(game);
    if (fcpe.isNotEmpty()) config.fcpeModelPath = juce::File(fcpe);
    config.gameModel = normalizedGameModel(properties->getValue("algorithm.gameModel", "medium"));
    // Older settings wrote "large" even when no model was installed. Adopt
    // the bundled default only in that case; keep working/custom selections.
    if (game.isEmpty() && config.gameModel == "large"
        && resolveGameDirectory(config) == juce::File{})
    {
        auto bundled = config;
        bundled.gameModel = "medium";
        if (resolveGameDirectory(bundled) != juce::File{}) config = bundled;
    }
    config.inference = inferenceFromId(properties->getIntValue("algorithm.inference", 1));
    const auto device = properties->getIntValue("algorithm.device", 1);
    config.deviceIndex = device <= 1 ? -1 : device - 2;
    return config;
}

AnalysisConfig AnalysisService::configFromEnvironment()
{
    AnalysisConfig config;
    config.gameModel = normalizedGameModel(juce::SystemStats::getEnvironmentVariable(
        "HACHISHIFTER_GAME_MODEL", "medium"));
    config.gameModelDirectory = environmentFile(config.gameModel == "small"
        ? "HACHISHIFTER_GAME_SMALL_MODEL_DIR" : "HACHISHIFTER_GAME_MODEL_DIR");
    auto fcpe = environmentFile("HACHISHIFTER_FCPE_ONNX");
    if (fcpe == juce::File{})
    {
        const auto directory = environmentFile("HACHISHIFTER_FCPE_MODEL_DIR");
        if (directory != juce::File{}) fcpe = directory.getChildFile("fcpe.onnx");
    }
    config.fcpeModelPath = fcpe;
    const auto inference = juce::SystemStats::getEnvironmentVariable(
        "HACHISHIFTER_INFERENCE", "auto").toLowerCase();
    config.inference = inference == "cpu" ? InferenceBackend::cpu
        : inference == "directml" ? InferenceBackend::directML
        : inference == "cuda" ? InferenceBackend::cuda
        : inference == "coreml" ? InferenceBackend::coreML
        : InferenceBackend::automatic;
    config.deviceIndex = juce::SystemStats::getEnvironmentVariable(
        "HACHISHIFTER_DEVICE", "-1").getIntValue();
    return config;
}

juce::File AnalysisService::resolveGameDirectory(const AnalysisConfig& config)
{
    const auto variant = normalizedGameModel(config.gameModel);
    const auto resolveRoot = [&](const juce::File& root) -> juce::File
    {
        if (root == juce::File{}) return {};
        if (isGameDirectory(root)) return root;
        auto candidate = root.getChildFile(variant);
        if (isGameDirectory(candidate)) return candidate;
        candidate = root.getChildFile("game").getChildFile(variant);
        if (isGameDirectory(candidate)) return candidate;
        // Continue accepting older flat model packs and explicit folders.
        candidate = root.getChildFile("game");
        if (isGameDirectory(candidate)) return candidate;
        return {};
    };

    if (auto selected = resolveRoot(config.gameModelDirectory); selected != juce::File{})
        return selected;
    const auto portable = executableDirectory().getChildFile("models").getChildFile("game");
    if (auto selected = resolveRoot(portable); selected != juce::File{}) return selected;
    return {};
}

juce::File AnalysisService::resolveFcpePath(const AnalysisConfig& config,
                                             const juce::File& gameDirectory)
{
    const auto normalize = [](juce::File value)
    {
        if (value.isDirectory()) value = value.getChildFile("fcpe.onnx");
        return value.existsAsFile() ? value : juce::File{};
    };
    if (auto selected = normalize(config.fcpeModelPath); selected != juce::File{})
        return selected;
    if (gameDirectory != juce::File{})
    {
        auto parent = gameDirectory.getParentDirectory();
        if (parent.getFileName().equalsIgnoreCase("game")) parent = parent.getParentDirectory();
        if (auto selected = normalize(parent.getChildFile("fcpe")); selected != juce::File{})
            return selected;
    }
    return normalize(executableDirectory().getChildFile("models").getChildFile("fcpe"));
}

AnalysisStatus AnalysisService::status(const AnalysisConfig& config)
{
    AnalysisStatus result;
    result.gameModel = normalizedGameModel(config.gameModel);
    result.requestedInference = inferenceBackendName(config.inference);
    result.activeInference = inferenceBackendName(resolvedInferenceBackend(config.inference));
    result.gameModelDirectory = resolveGameDirectory(config);
    if (result.gameModelDirectory != juce::File{})
    {
        const auto metadata = juce::JSON::parse(result.gameModelDirectory
            .getChildFile("model-info.json").loadFileAsString());
        auto actualVariant = metadata.getProperty("variant", {}).toString().toLowerCase();
        if (actualVariant.isEmpty()) actualVariant = result.gameModelDirectory.getFileName().toLowerCase();
        if (actualVariant == "small" || actualVariant == "medium" || actualVariant == "large")
            result.gameModel = actualVariant;
    }
    result.fcpeModelPath = resolveFcpePath(config, result.gameModelDirectory);
    result.gameModelReady = result.gameModelDirectory != juce::File{};
    result.fcpeModelReady = result.fcpeModelPath != juce::File{};
    result.onnxRuntimeReady = GameAnalyzer::runtimeAvailable();
    if (result.gameModelReady && result.onnxRuntimeReady)
    {
        result.activeBackend = result.fcpeModelReady ? "GAME+FCPE" : "GAME+native-hq";
        result.message = "GAME " + result.gameModel
            + (result.fcpeModelReady ? " ready; FCPE model ready"
                                     : " ready; FCPE model missing")
            + "; inference=" + result.activeInference;
    }
    else
    {
        juce::StringArray missing;
        if (!result.gameModelReady) missing.add("GAME " + result.gameModel + " model pack");
        if (!result.fcpeModelReady) missing.add("FCPE model");
        if (!result.onnxRuntimeReady) missing.add("ONNX analysis runtime");
        result.message = "native-hq fallback; missing: " + missing.joinIntoString(", ");
        if (config.inference != resolvedInferenceBackend(config.inference))
            result.message << "; requested " << result.requestedInference
                           << " is not packaged, using CPU";
    }
    return result;
}

AnalysisResult AnalysisService::analyse(const juce::File& file,
                                        const AnalysisConfig& config,
                                        juce::String& error,
                                        Progress progress)
{
    AnalysisResult result;
    result.status = status(config);
    if (result.status.gameModelReady && result.status.onnxRuntimeReady)
    {
        GameAnalyzer::Options options;
        options.performanceMode = result.status.gameModel == "small";
        // GAME keeps four sessions and FCPE a fifth. Giving every session all
        // logical CPUs oversubscribes interactive imports and leaves no room
        // for the editor/audio callback. More threads also slow these small ops.
        options.intraOpThreads = juce::jlimit(1, 4, juce::SystemStats::getNumCpus() / 2);
        options.inference = config.inference;
        options.deviceIndex = config.deviceIndex;
        juce::String gameError;
        auto game = GameAnalyzer::analyse(file, result.status.gameModelDirectory,
                                          result.status.fcpeModelPath,
                                          options, gameError, progress);
        result.notes = std::move(game.notes);
        if (!result.notes.empty())
        {
            result.status.activeBackend = game.fcpeUsed ? "GAME+FCPE" : "GAME+native-hq";
            result.status.activeInference = game.activeInference;
            result.warning = game.warning;
            if (!game.fcpeUsed && !result.status.fcpeModelReady)
                result.warning = "FCPE model missing; GAME uses native-hq source F0";
            error.clear();
            return result;
        }
        result.warning = gameError;
    }
    result.notes = NativeAnalyzer::analyse(file, error, std::move(progress));
    result.status.activeBackend = "native-hq";
    if (result.warning.isEmpty() && result.status.gameModelReady
        && !result.status.onnxRuntimeReady)
        result.warning = result.status.message;
    return result;
}

std::size_t AnalysisService::applySourcePitch(ClipData& clip,
                                               const std::vector<NoteData>& sourceNotes)
{
    if (sourceNotes.empty()) return 0;
    clip.nativeSourcePitch=nativeSourcePitchReference(sourceNotes,[](double t){return t;});
    clip.nativeSourcePitchComplete=true;
    const auto audio = nativeAudioPreviewClip(clip);
    const auto map = nativeSourceTimeMap(audio);
    auto updated = std::size_t(0);
    for (auto& note : clip.notes)
    {
        if (note.nativeUnpitched)
        {
            note.sourceMidiCenter = note.midiNote;
            note.sourcePitchMeasured = true;
            note.contour = {{0, 0, 0, false}, {note.durationSeconds, 0, 0, false}};
            note.pitchControlPoints.clear();
            ++updated;
            continue;
        }
        const auto oldContour = note.contour;
        std::vector<PitchPoint> contour;
        std::vector<std::optional<std::pair<float, float>>> samples;
        std::vector<float> voicedPitch;
        const auto sample = [&](double local)
        {
            PitchPoint point;
            point.timeSeconds = local;
            const auto target = note.startSeconds + local - clip.audioStartSeconds;
            auto pitch = target >= -1.0e-9 && target <= audio.durationSeconds + 1.0e-9
                ? absolutePitchAt(sourceNotes, audio.sourceOffsetSeconds
                    + nativeSourceTimeAt(map, target)) : std::nullopt;
            if (pitch) voicedPitch.push_back(pitch->first);
            samples.push_back(pitch);
            // A source refresh replaces acoustic information, never an edit.
            if (!oldContour.empty())
            {
                auto right = std::lower_bound(oldContour.begin(), oldContour.end(), local,
                    [](const auto& p, double t) { return p.timeSeconds < t; });
                if (right == oldContour.end()) right = oldContour.end() - 1;
                const auto left = right != oldContour.begin() && right->timeSeconds > local
                    ? right - 1 : right;
                if (left->hasManualTarget && right->hasManualTarget)
                {
                    const auto span = right->timeSeconds - left->timeSeconds;
                    const auto u = span > 1.0e-9 ? juce::jlimit(0.0, 1.0,
                        (local - left->timeSeconds) / span) : 0.0;
                    point.hasManualTarget = true;
                    point.manualTargetCents = left->manualTargetCents
                        + static_cast<float>(u) * (right->manualTargetCents - left->manualTargetCents);
                }
            }
            contour.push_back(point);
        };
        for (double local = 0.0; local < note.durationSeconds - 1.0e-9; local += 0.005)
            sample(local);
        sample(note.durationSeconds);
        const auto hasMeasuredPitch = !voicedPitch.empty();
        const auto centreCents = !hasMeasuredPitch
            ? (note.sourceMidiCenter >= 0.0f ? note.sourceMidiCenter : note.midiNote) * 100.0f
            : median(std::move(voicedPitch));
        // Timing-only OTO/HJM rows have no pitch centre. Their old C4 value is
        // a placeholder; begin at the measured pitch, retaining transposition.
        if (hasMeasuredPitch && note.sourceMidiCenter < 0.0f && note.pitchControlPoints.empty()
            && std::none_of(oldContour.begin(), oldContour.end(),
                [](const auto& p) { return p.hasManualTarget; }))
            note.midiNote = juce::jlimit(0.0f, 127.0f, note.midiNote + centreCents / 100.0f - 60.0f);
        if (hasMeasuredPitch)
            note.sourceMidiCenter = juce::jlimit(0.0f, 127.0f, centreCents / 100.0f);
        for (std::size_t i = 0; i < contour.size(); ++i)
        {
            auto& point = contour[i];
            point.voiced = samples[i].has_value();
            if (!point.voiced) continue;
            point.relativeCents = samples[i]->first - centreCents;
            point.withoutVibratoCents = samples[i]->second - centreCents;
        }
        note.contour = std::move(contour);
        note.sourcePitchMeasured = true;
        ++updated;
    }
    return updated;
}

bool AnalysisService::reanalyseProjectSourcePitch(ProjectData& project,
                                                   const AnalysisConfig& config,
                                                   juce::String& error,
                                                   Progress progress,
                                                   AnalysisStatus* usedStatus)
{
    std::map<juce::String, juce::File> files;
    for (const auto& track : project.tracks)
        for (const auto& clip : track.clips)
            for (const auto* source : clipSourceRegions(clip))
                if (source->sourceFile.existsAsFile())
                    files.try_emplace(source->sourceFile.getFullPathName(), source->sourceFile);
    if (files.empty())
    {
        error = "Source-pitch reanalysis has no readable media";
        return false;
    }
    std::map<juce::String, std::vector<NoteData>> analyses;
    juce::StringArray failures;
    auto fileIndex = std::size_t(0);
    AnalysisStatus current = status(config);
    for (const auto& [path, file] : files)
    {
        juce::String localError;
        auto analysed = analyse(file, config, localError, [&](double value)
        {
            if (progress) progress((static_cast<double>(fileIndex) + value)
                                   / static_cast<double>(files.size()));
        });
        if (!analysed.notes.empty())
        {
            current = analysed.status;
            analyses.emplace(path, std::move(analysed.notes));
        }
        else failures.add(file.getFileName() + ": " + localError);
        ++fileIndex;
    }
    auto updatedNotes = std::size_t(0);
    for (auto& track : project.tracks)
        for (auto& clip : track.clips)
        {
            auto parts = expandedClipParts(clip);
            for (auto& part : parts)
            {
                const auto found = analyses.find(part.sourceFile.getFullPathName());
                if (found == analyses.end()) continue;
                updatedNotes += applySourcePitch(part, found->second);
                for (auto& measured : part.notes)
                    if (const auto note = std::find_if(clip.notes.begin(), clip.notes.end(),
                        [&](const auto& n) { return n.id == measured.id; }); note != clip.notes.end())
                    {
                        note->sourceMidiCenter = measured.sourceMidiCenter;
                        note->sourcePitchMeasured = measured.sourcePitchMeasured;
                        note->midiNote = measured.midiNote;
                        note->contour = std::move(measured.contour);
                    }
            }
        }
    if (usedStatus != nullptr) *usedStatus = current;
    if (progress) progress(1.0);
    if (!failures.isEmpty()) error = failures.joinIntoString("\n");
    if (updatedNotes == 0)
    {
        if (error.isEmpty()) error = "Source-pitch reanalysis produced no aligned contours";
        return false;
    }
    return true;
}

juce::String AnalysisService::backendText(const AnalysisStatus& value)
{
    auto text = value.activeBackend;
    if (value.activeBackend.startsWith("GAME+"))
        text << " (GAME " << value.gameModel
             << ", " << value.activeInference << ")";
    return text;
}
}
