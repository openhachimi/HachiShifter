#include "MaterialPronunciationCatalog.h"
#include "OttoMaterialIO.h"
#include "OttoMaterialMigration.h"
#include "NativeMaterialSelectionPlan.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <memory>
#include <set>

namespace hachi
{
namespace
{
using Catalog = MaterialPronunciationCatalog;
using juce::File;
using juce::String;
String key(const File& file)
{
    auto value = file.getFullPathName().replaceCharacter('\\', '/');
#if JUCE_WINDOWS
    value = value.toLowerCase();
#endif
    return value;
}
bool inside(const File& file, const File& folder) { return file.isAChildOf(folder); }
bool hasAlias(const SampleRegionSetting& row, const String& alias)
{ return row.name == alias || row.additionalAliases.contains(alias); }
Catalog::Format formatFor(UtauMode mode)
{
    return mode == UtauMode::classic ? Catalog::Format::classicOto
        : mode == UtauMode::jie ? Catalog::Format::jieOto : Catalog::Format::mouOto;
}
void append(std::vector<Catalog::Entry>& entries, const File& audio, const Catalog::Root& root,
            const SampleRegionSetting& row, Catalog::Format format, const String& fingerprint)
{
    Catalog::Entry entry;
    entry.audio = audio; entry.root = root.folder;
    entry.manifest = root.folder.getChildFile("otto.ini");
    entry.audioId = row.materialAudioId; entry.regionId = row.materialRegionId;
    entry.row = row; entry.format = format; entry.otoMode = root.otoMode;
    entry.sourceFingerprint = fingerprint;
    entry.alias = row.name;
    if (entry.alias.isNotEmpty()) entries.push_back(entry);
    for (const auto& alias : row.additionalAliases)
        if (alias.isNotEmpty() && alias != row.name)
        { entry.alias = alias; entries.push_back(entry); }
}
}

Catalog::Result Catalog::index(const std::vector<Root>& requested, Cancel cancel)
{
    Result result;
    std::map<String, Root> unique;
    std::set<String> rejected;
    for (const auto& root : requested)
    {
        if (root.folder == File{} || !root.folder.isDirectory())
        { result.errors.add("Material root is not a directory: " + root.folder.getFullPathName()); continue; }
        if (root.otoMode != UtauMode::classic && root.otoMode != UtauMode::jie && root.otoMode != UtauMode::mou)
        {
            result.errors.add("Unsupported OTO mode: " + root.folder.getFullPathName());
            rejected.insert(key(root.folder));
            unique.emplace(key(root.folder), root); // ownership still blocks parent fallback
            continue;
        }
        auto [it, added] = unique.emplace(key(root.folder), root);
        if (!added && it->second.otoMode != root.otoMode)
        {
            result.errors.add("Duplicate material root has conflicting OTO modes: " + root.folder.getFullPathName());
            rejected.insert(key(root.folder));
        }
    }
    const auto cancelled = [&]()
    {
        if (cancel && cancel()) { result.cancelled = true; result.entries.clear(); return true; }
        return false;
    };
    const auto owns = [&](const File& audio, const Root& root)
    {
        if (!inside(audio, root.folder)) return false;
        for (const auto& [path, other] : unique)
            if (path.length() > key(root.folder).length() && inside(other.folder, root.folder)
                && inside(audio, other.folder)) return false;
        return true;
    };
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    // Local header durations are shared by aliases. No reader->read() calls.
    std::map<String, double> durations;
    for (const auto& [rootKey, root] : unique)
    {
        if (rejected.count(rootKey)) continue;
        if (cancelled()) return result;
        std::vector<Entry> rootEntries;
        const auto manifest = root.folder.getChildFile("otto.ini");
        if (manifest.exists())
        {
            OttoMaterialIO::Document document;
            String error;
            if (!OttoMaterialIO::loadDocument(manifest, document, error))
            { result.errors.add(error); continue; } // Invalid OTTO NEVER permits OTO fallback.
            for (const auto& recording : document.audio)
            {
                if (cancelled()) return result;
                const auto audio = root.folder.getChildFile(recording.relativePath);
                if (!owns(audio, root)) continue;
                if (!audio.existsAsFile())
                { result.warnings.add("Missing annotated recording: " + audio.getFullPathName()); continue; }
                for (const auto& row : recording.rows)
                    append(rootEntries, audio, root, row, Format::otto, {});
            }
        }
        else
        {
            // Per-file parser avoids appendUnlistedAudio and any fabricated
            // filename pronunciation. Each governing OTO is parsed once/root.
            std::map<String, File> files;
            for (const auto& item : juce::RangedDirectoryIterator(root.folder, true,
                     root.otoMode == UtauMode::jie ? "oto.ini;oto.jie.ini" : "oto.ini", File::findFiles))
            {
                if (cancelled()) return result;
                const auto classic = item.getFile().getSiblingFile("oto.ini");
                if (owns(classic, root)) files.emplace(key(classic), classic);
            }
            std::map<String, std::vector<VoicebankOtoEntry>> byAudio;
            std::map<String, OttoMaterialMigration::OtoSource> sources;
            bool failed = false;
            for (const auto& [path, file] : files)
            {
                juce::ignoreUnused(path);
                if (cancelled()) return result;
                juce::StringArray warnings;
                auto entries = SampleSettings::loadVoicebankOto(root.folder, warnings,
                    root.otoMode == UtauMode::jie, root.otoMode == UtauMode::mou, file);
                result.warnings.addArray(warnings);
                if (!warnings.isEmpty())
                {
                    result.errors.add("OTO source reported errors: " + file.getFullPathName()
                                      + ": " + warnings.joinIntoString("; "));
                    failed = true; break;
                }
                for (const auto& entry : entries)
                {
                    if (entry.lineIndex < 0 || !owns(entry.audioFile, root)) continue;
                    // Only ancestors can govern, identical to loadAudioOto.
                    if (!inside(entry.audioFile, entry.otoFile.getParentDirectory())) continue;
                    byAudio[key(entry.audioFile)].push_back(entry);
                }
            }
            if (failed) continue;
            for (const auto& [audioKey, entries] : byAudio)
            {
                if (cancelled()) return result;
                const auto audio = entries.front().audioFile;
                if (!audio.existsAsFile())
                { result.warnings.add("Missing OTO recording: " + audio.getFullPathName()); continue; }
                int nearest = 0;
                for (const auto& entry : entries)
                    nearest = std::max(nearest, key(entry.otoFile.getParentDirectory()).length());
                for (const auto& entry : entries)
                {
                    if (key(entry.otoFile.getParentDirectory()).length() != nearest) continue;
                    if (cancelled()) return result;
                    String error;
                    auto [source, added] = sources.try_emplace(key(entry.otoFile));
                    if (added && !OttoMaterialMigration::readOtoSource(entry, root.otoMode, source->second, error))
                    { result.errors.add(error); failed = true; break; }
                    // Use the converter's exact operation order: an equivalent
                    // expression can round below end and spuriously fail bounds.
                    double duration = entry.offsetMs / 1000.0 - entry.cutoffMs / 1000.0;
                    if (entry.cutoffMs >= 0)
                    {
                        auto found = durations.find(audioKey);
                        if (found == durations.end())
                        {
                            std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(audio));
                            if (!reader || !std::isfinite(reader->sampleRate) || reader->sampleRate <= 0
                                || reader->lengthInSamples <= 0 || reader->numChannels == 0)
                            { result.errors.add("Unreadable or empty recording: " + audio.getFullPathName()); failed = true; break; }
                            found = durations.emplace(audioKey, static_cast<double>(reader->lengthInSamples) / reader->sampleRate).first;
                        }
                        duration = found->second;
                    }
                    SampleRegionSetting row;
                    if (!OttoMaterialMigration::convertOtoEntry(entry, root.otoMode, duration, source->second, row, error))
                    { result.errors.add(error); failed = true; break; }
                    // An empty classic alias means filename fallback in UTAU;
                    // this catalog intentionally never advertises that inference.
                    if (entry.alias.isNotEmpty())
                        append(rootEntries, audio, root, row, formatFor(root.otoMode), entry.sourceFingerprint);
                }
                if (failed) break;
            }
            if (failed) continue; // No partial root result after a conversion error.
        }
        result.entries.insert(result.entries.end(), rootEntries.begin(), rootEntries.end());
    }
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry& a, const Entry& b)
    {
        if (key(a.root) != key(b.root)) return key(a.root) < key(b.root);
        if (key(a.audio) != key(b.audio)) return key(a.audio) < key(b.audio);
        if (a.audioId != b.audioId) return a.audioId < b.audioId;
        if (a.regionId != b.regionId) return a.regionId < b.regionId;
        return a.alias < b.alias;
    });
    return result;
}

bool Catalog::continuousRows(const std::vector<SampleRegionSetting>& rows,const String& leftId,const String& rightId,String& error)
{
    error.clear();constexpr double epsilon=1.e-8;
    const auto a=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.materialRegionId==leftId;});
    const auto b=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.materialRegionId==rightId;});
    if(a==rows.end()||b==rows.end()||a==b||a->materialAudioId.isEmpty()||a->materialAudioId!=b->materialAudioId)
    {error="Missing or incompatible source-row identity";return false;}
    if(a->pronunciationKind==PronunciationKind::transition||b->pronunciationKind==PronunciationKind::transition)
    {error="A transition pronunciation is completely cut, not a source-continuity chain";return false;}
    if(b->regionStartSeconds<=a->regionStartSeconds+epsilon||b->alignmentSeconds<=a->alignmentSeconds+epsilon)
    {error="Source row order is not strictly chronological";return false;}
    double nextStart=std::numeric_limits<double>::infinity();for(const auto& row:rows)if(row.regionStartSeconds>a->regionStartSeconds+epsilon)nextStart=std::min(nextStart,row.regionStartSeconds);
    if(std::abs(b->regionStartSeconds-nextStart)>epsilon||(a->connectedNextRegionId.isNotEmpty()&&a->connectedNextRegionId!=rightId)
        ||(b->regionStartSeconds>a->regionEndSeconds+epsilon&&a->connectedNextRegionId!=rightId))
    {error="The source rows are not an explicitly contiguous ordered chain";return false;}
    NativeMaterialSelectionPlan proof;return planNativeMaterialSelection(rows,{leftId,rightId},proof,error);
}

bool Catalog::matchSequences(const std::vector<Entry>& entries,const juce::StringArray& lyrics,
    std::vector<Sequence>& output,String& error,Cancel cancel)
{
    error.clear();std::vector<Sequence> result;if(lyrics.size()<2){output.clear();return true;}
    constexpr double epsilon=1.e-8;constexpr std::size_t maximumChoices=4096;
    struct Row{SampleRegionSetting row;std::vector<const Entry*> aliases;};
    std::map<String,std::map<String,Row>> groups;
    for(const auto& entry:entries)
    {
        if(cancel&&cancel()){error="Pronunciation sequence search cancelled";return false;}
        const auto groupKey=key(entry.root)+"\n"+key(entry.audio)+"\n"+entry.audioId+"\n"+String(int(entry.format));
        auto& row=groups[groupKey][entry.regionId];if(row.aliases.empty())row.row=entry.row;row.aliases.push_back(&entry);
    }
    for(const auto& [groupId,byId]:groups)
    {
        juce::ignoreUnused(groupId);std::vector<const Row*> ordered;std::vector<SampleRegionSetting> fullRows;
        for(const auto& [id,row]:byId){juce::ignoreUnused(id);ordered.push_back(&row);fullRows.push_back(row.row);}
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->row.regionStartSeconds<b->row.regionStartSeconds;});
        const auto continues=[&](const SampleRegionSetting& left,const SampleRegionSetting& right)
        {
            String why;return continuousRows(fullRows,left.materialRegionId,right.materialRegionId,why);
        };
        for(const auto* first:ordered)for(const auto* alias:first->aliases)
        {
            if(!alias->alias.equalsIgnoreCase(lyrics[0].trim()))continue;
            std::vector<Sequence> prefixes{{{*alias}}};
            for(int lyric=1;lyric<lyrics.size()&&!prefixes.empty();++lyric)
            {
                if(cancel&&cancel()){error="Pronunciation sequence search cancelled";return false;}
                std::vector<Sequence> next;
                for(const auto& prefix:prefixes)
                {
                    const auto& previous=prefix.members.back();double nextStart=std::numeric_limits<double>::infinity();
                    for(const auto* row:ordered)if(row->row.regionStartSeconds>previous.row.regionStartSeconds+epsilon)nextStart=std::min(nextStart,row->row.regionStartSeconds);
                    for(const auto* row:ordered)
                    {
                        if(std::abs(row->row.regionStartSeconds-nextStart)>epsilon||!continues(previous.row,row->row))continue;
                        for(const auto* match:row->aliases)if(match->alias.equalsIgnoreCase(lyrics[lyric].trim()))
                        {
                            auto choice=prefix;choice.members.push_back(*match);result.push_back(choice);next.push_back(std::move(choice));
                            if(result.size()>maximumChoices){error="Too many explicit source-chain choices; reduce the selected score range";return false;}
                        }
                    }
                }
                prefixes=std::move(next);
            }
        }
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.members.size()>b.members.size();});
    output=std::move(result);return true;
}

bool Catalog::resolve(const Entry& selected, Entry& current, String& error,
                      std::vector<SampleRegionSetting>* fullRows)
{
    error.clear();
    const bool native = selected.manifest.exists();
    if (selected.manifest != selected.root.getChildFile("otto.ini")
        || (native != (selected.format == Format::otto)))
    { error = "Governing annotation format changed; rebuild pronunciation catalog"; return false; }
    std::vector<SampleRegionSetting> rows;
    if (!OttoMaterialMigration::readAudioAnnotations(selected.audio, selected.root,
                                                   selected.otoMode, rows, error, selected.audioId)) return false;
    for (const auto& row : rows)
        if (row.materialAudioId == selected.audioId && row.materialRegionId == selected.regionId
            && hasAlias(row, selected.alias))
        {
            auto resolved = selected;
            resolved.row = row;
            current = std::move(resolved);
            if (fullRows) *fullRows = std::move(rows);
            return true;
        }
    error = "Pronunciation source changed or alias/identity disappeared; rebuild pronunciation catalog";
    return false;
}
}
