#include "MelodyneProvider.h"

#include <memory>

namespace hachi::backend
{
namespace
{
std::vector<juce::File> roots()
{
    std::vector<juce::File> result;
#if JUCE_WINDOWS
    result.emplace_back(juce::File("C:/Program Files/Celemony"));
    result.emplace_back(juce::File("C:/Program Files/Common Files/Celemony"));
    result.emplace_back(juce::File("C:/Program Files/Common Files/VST3/Celemony"));
    result.emplace_back(juce::File("C:/Program Files (x86)/Celemony"));
#else
    // WSL exposes Windows drives through /mnt.  Native Windows builds use the
    // same relative candidates with the C:/ paths above.
    result.emplace_back(juce::File("/mnt/c/Program Files/Celemony"));
    result.emplace_back(juce::File("/mnt/c/Program Files/Common Files/Celemony"));
    result.emplace_back(juce::File("/mnt/c/Program Files/Common Files/VST3/Celemony"));
    result.emplace_back(juce::File("/mnt/c/Program Files (x86)/Celemony"));
#endif
    return result;
}

void collect(const juce::File& directory, MelodyneInstallation& best)
{
    if (!directory.isDirectory()) return;
    for (const auto& child : juce::RangedDirectoryIterator(directory, false))
    {
        const auto file = child.getFile();
        if (file.isDirectory())
        {
            collect(file, best);
            continue;
        }
        const auto name = file.getFileName().toLowerCase();
        if (name == "melodyne.exe" && !best.executable.existsAsFile())
        {
            best.executable = file;
            best.version = file.getParentDirectory().getFileName();
        }
        else if (name == "melodyne.vst3" && !best.vst3.exists())
            best.vst3 = file;
        else if (name.startsWith("melodynecore-") && name.endsWith(".dll")
                 && !best.coreBundle.existsAsFile())
            best.coreBundle = file;
    }
}
}

std::optional<MelodyneInstallation> MelodyneProvider::detect()
{
    MelodyneInstallation result;
    for (const auto& root : roots()) collect(root, result);
    return result.isUsableCandidate() ? std::optional { result } : std::nullopt;
}

bool MelodyneProvider::nativeImportAvailable()
{
    // Discovery alone is not a supported API integration.
    return false;
}

MelodyneVst3Probe MelodyneProvider::probeVst3()
{
    MelodyneVst3Probe result;
    const auto installation = detect();
    if (!installation || !installation->vst3.exists())
    {
        result.detail = "Melodyne VST3 not detected";
        return result;
    }
    result.candidateFound = true;
#if JUCE_WINDOWS
    result.hostPlatformSupported = true;
    juce::AudioPluginFormatManager formats;
    formats.addDefaultFormats();
    juce::OwnedArray<juce::PluginDescription> types;
    juce::AudioPluginFormat* format = nullptr;
    for (int index = 0; index < formats.getNumFormats(); ++index)
        if (formats.getFormat(index)->getName() == "VST3")
            format = formats.getFormat(index);
    if (format == nullptr)
    {
        result.detail = "VST3 host format unavailable";
        return result;
    }
    format->findAllTypesForFile(types, installation->vst3.getFullPathName());
    if (types.isEmpty())
    {
        result.detail = "Melodyne VST3 exposes no loadable plugin description";
        return result;
    }
    result.description = *types[0];
    result.pluginDescribed = true;
    result.detail = "Melodyne VST3 plugin description discovered";
#else
    result.detail = "Windows VST3 detected, but this process is not Windows";
#endif
    return result;
}

void MelodyneProvider::createVst3InstanceAsync(double sampleRate, int blockSize,
                                                Vst3InstanceCallback callback)
{
    auto probe = probeVst3();
    if (!probe.pluginDescribed)
    {
        callback({}, probe.detail);
        return;
    }
#if JUCE_WINDOWS
    auto formats = std::make_shared<juce::AudioPluginFormatManager>();
    formats->addDefaultFormats();
    formats->createPluginInstanceAsync(probe.description, sampleRate, blockSize,
        [formats, callback = std::move(callback)](std::unique_ptr<juce::AudioPluginInstance> instance,
                                                   const juce::String& error)
        {
            callback(std::move(instance), error);
        });
#else
    juce::ignoreUnused(sampleRate, blockSize);
    callback({}, probe.detail);
#endif
}

bool MelodyneProvider::nativeRenderAvailable()
{
    return false;
}

bool MelodyneProvider::experimentalSelfImportEnabled()
{
    // UI testing uses the independent MPD reader until native import is ready.
    return true;
}

bool MelodyneProvider::experimentalMergedRenderEnabled()
{
    return false;
}

juce::String MelodyneProvider::statusText()
{
    const auto found = detect();
    if (!found) return "Melodyne not detected";
    const auto version = found->version.isNotEmpty() ? " (" + found->version + ")" : juce::String();
    return "Melodyne detected" + version + "; supported provider unavailable";
}
}
