#include "JapaneseAutoAnnotation.h"
#include "OttoMaterialIO.h"
#include "PronunciationTiming.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <set>
#include <cmath>

namespace hachi
{
namespace
{
bool vowel(const juce::String& phone)
{ return phone == "a" || phone == "i" || phone == "u" || phone == "e" || phone == "o" || phone == "N" || phone == "um"; }
juce::String digest(const juce::String& text)
{ const auto bytes = text.toUTF8(); return juce::SHA256(bytes.getAddress(), std::size_t(bytes.sizeInBytes() - 1)).toHexString(); }
}

juce::File JapaneseAutoAnnotation::defaultModelRoot()
{
    return juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory()
        .getChildFile("models").getChildFile("japanese_annotation");
}

bool JapaneseAutoAnnotation::planMode(InferenceMode mode,ModePlan& output,juce::String& error)
{
    error.clear();ModePlan plan;
    switch(mode)
    {
        case InferenceMode::recognizeAndAlign:plan={true,true,true};break;
        case InferenceMode::recognizeOnly:plan={true,false,false};break;
        case InferenceMode::alignOnly:plan={false,true,true};break;
        default:error="Unsupported Japanese inference mode";return false;
    }
    output=plan;return true;
}

bool JapaneseAutoAnnotation::phonemesToSyllables(const juce::StringArray& recognized,
    const juce::File& dictionary, PhoneSequence& output, juce::String& error)
{
    if (!dictionary.existsAsFile() || dictionary.getSize() > 1024 * 1024)
    { error = "The official Japanese TIFA dictionary is missing or oversized"; return false; }
    std::map<juce::String, juce::StringArray> entries;
    std::map<juce::String, juce::StringArray> inverse;
    std::set<juce::String> atomicPhones;
    int longest = 0;
    for (const auto& raw : juce::StringArray::fromLines(dictionary.loadFileAsString()))
    {
        const auto line = raw.trim();
        if (line.isEmpty() || line.startsWithChar('#')) continue;
        auto words = juce::StringArray::fromTokens(line, " \t", ""); words.removeEmptyStrings();
        if (words.size() < 2) { error = "Malformed Japanese dictionary row"; return false; }
        const auto alias = words[0]; words.remove(0);
        if (entries.contains(alias)) { error = "Duplicate Japanese dictionary spelling"; return false; }
        entries.emplace(alias, words); inverse[words.joinIntoString(" ")].addIfNotAlreadyThere(alias);
        for (const auto& phone : words) atomicPhones.insert(phone);
        longest = juce::jmax(longest, words.size());
    }
    if (entries.empty()) { error = "Japanese dictionary is empty"; return false; }
    // The ASR model's own supported romaji spellings give a canonical name
    // for dictionary homophones (re/le, ru/lu). This is lexicon normalization,
    // never a choice of a physical recording/annotation variant by ordinal.
    std::set<juce::String> canonicalSpellings;
    const auto asrVocabulary=dictionary.getParentDirectory().getSiblingFile("asr").getChildFile("phoneme_vocab.json");
    if(asrVocabulary.existsAsFile())
    {
        const auto catalog=juce::JSON::parse(asrVocabulary);
        if(const auto* object=catalog.getDynamicObject())
            for(const auto& item:object->getProperties())canonicalSpellings.insert(item.name.toString());
    }
    PhoneSequence sequence;
    // Preserve an explicitly recognized dictionary spelling (e.g. re rather
    // than its same-phone alternative le), instead of losing it on expansion.
    std::map<int,std::pair<int,juce::String>> recognizedGroups;
    for (const auto& token : recognized)
    {
        if (token == "SOS" || token == "EOS") continue;
        if (const auto it = entries.find(token); it != entries.end())
        {
            recognizedGroups.emplace(sequence.phones.size(),std::make_pair(it->second.size(),token));
            sequence.phones.addArray(it->second);
        }
        else if (atomicPhones.contains(token)) sequence.phones.add(token);
        else { error = "Japanese token has no official dictionary/phone mapping: " + token + ". Correct the recognized sequence before publishing."; return false; }
    }
    if (sequence.phones.isEmpty() || sequence.phones.size() > 1024)
    { error = "Japanese phone sequence is empty or too long"; return false; }
    for (int index = 0; index < sequence.phones.size();)
    {
        int count = 0; juce::String alias;
        for (int n = juce::jmin(longest, sequence.phones.size() - index); n > 0; --n)
        {
            juce::StringArray part;
            for (int j = 0; j < n; ++j) part.add(sequence.phones[index + j]);
            if (const auto it = inverse.find(part.joinIntoString(" ")); it != inverse.end())
            {
                const auto recognizedGroup=recognizedGroups.find(index);
                if(recognizedGroup!=recognizedGroups.end()&&recognizedGroup->second.first==n
                    &&it->second.contains(recognizedGroup->second.second))
                    alias=recognizedGroup->second.second;
                else if(it->second.size()==1)alias=it->second[0];
                else
                {
                    juce::StringArray canonical;
                    for(const auto& spelling:it->second)if(canonicalSpellings.contains(spelling))canonical.add(spelling);
                    if(canonical.size()==1)alias=canonical[0];
                    else { error = "Ambiguous Japanese pronunciation group: " + part.joinIntoString(" ") + ". Supply its explicit dictionary spelling."; return false; }
                }
                count = n; break;
            }
        }
        if (!count)
        { error = "Incomplete Japanese pronunciation at phone " + juce::String(index + 1) + ": " + sequence.phones[index]; return false; }
        Syllable syllable; syllable.alias = alias; syllable.firstPhone = index; syllable.phoneCount = count;
        for (int j = 0; j < count; ++j)
        {
            sequence.groups.push_back(std::int64_t(sequence.syllables.size() + 1));
            if (syllable.vowelPhone < 0 && vowel(sequence.phones[index + j])) syllable.vowelPhone = index + j;
        }
        sequence.syllables.push_back(std::move(syllable)); index += count;
    }
    output = std::move(sequence); return true;
}

bool JapaneseAutoAnnotation::preview(const juce::File& file,const juce::File& root,
    const backend::OrtExecutionConfig& execution,Result& output,juce::String& error,
    const backend::AnnotationCancel& cancel,const std::function<void(const juce::String&)>& progress,
    const juce::StringArray& correctedTokens)
{
    return infer(file,root,execution,correctedTokens.isEmpty()?InferenceMode::recognizeAndAlign:InferenceMode::alignOnly,
        correctedTokens,output,error,cancel,progress);
}

bool JapaneseAutoAnnotation::infer(const juce::File& file,const juce::File& configuredRoot,
    const backend::OrtExecutionConfig& execution,InferenceMode mode,const juce::StringArray& manualTokens,
    Result& output,juce::String& error,const backend::AnnotationCancel& cancel,
    const std::function<void(const juce::String&)>& progress,const std::function<void(const Result&)>& recognitionReady,
    const std::function<void(const Result&)>& stageDiagnostics)
{
    error.clear();ModePlan stages;if(!planMode(mode,stages,error))return false;
    auto supplied=manualTokens;supplied.trim();supplied.removeEmptyStrings();
    if(mode==InferenceMode::alignOnly&&supplied.isEmpty())
    {error="Enter a non-empty Japanese syllable/phone sequence before alignment-only inference";return false;}
    const auto root = configuredRoot == juce::File{} ? defaultModelRoot() : configuredRoot;
    const auto asrRoot = root.getChildFile("asr"), tifaRoot = root.getChildFile("tifa");
    if (progress) progress("Reading recording");
    backend::AnnotationAudio audio;
    if (!backend::AnnotationAudio::load(file, audio, error, cancel)) return false;
    Result result;result.mode=mode;result.stagePlan=stages;result.audioLoadCount=1;
    result.audio = file; result.sourceFingerprint = audio.fingerprint;
    if(stageDiagnostics)stageDiagnostics(result);
    if (audio.mono.getMagnitude(0, audio.mono.getNumSamples()) < 1.e-7f)
    { error = "Recording is silent; no automatic annotations were created"; return false; }
    if (stages.runRecognition)
    {
        if (progress) progress("Recognizing Japanese phonemes (native ONNX)");
        backend::JapaneseAsrResult asr;++result.asrCallCount;
        if(stageDiagnostics)stageDiagnostics(result);
        if (!backend::JapanesePhonemeAsr::transcribe(audio, asrRoot, execution, asr, error, cancel)) return false;
        result.recognizedTokens = asr.tokens; result.asrBackend = asr.activeBackend;
        result.asrModelFingerprint = asr.modelFingerprint;
    }
    else
    {
        result.manualTokens = supplied;
        result.warnings.add("Alignment-only mode uses the explicitly supplied sequence; the ASR model was not run.");
    }
    const auto dictionary = tifaRoot.getChildFile("japanese_dict_full.txt");
    const auto dictionaryHash = juce::SHA256(dictionary).toHexString();result.dictionaryFingerprint=dictionaryHash;
    const auto& inputTokens=stages.runRecognition?result.recognizedTokens:result.manualTokens;
    if (!phonemesToSyllables(inputTokens, dictionary, result.sequence, error)) return false;
    for(const auto& syllable:result.sequence.syllables)result.normalizedSyllables.add(syllable.alias);
    result.audioId="ja_text_"+digest(file.getFullPathName()+"|"+audio.fingerprint+"|"+result.normalizedSyllables.joinIntoString(" ")+"|"+dictionaryHash).substring(0,28);
    const auto inputsUnchanged=[&]()
    {
        if(cancel&&cancel()){error="Annotation cancelled";return false;}
        if(juce::SHA256(dictionary).toHexString()!=dictionaryHash){error="Japanese dictionary changed during annotation";return false;}
        if(juce::SHA256(file).toHexString()!=audio.fingerprint){error="Recording changed while annotation was running";return false;}
        return true;
    };
    if(!inputsUnchanged())return false;
    if(recognitionReady)recognitionReady(result); // unaligned, ready=false, rows empty
    if(!stages.runAlignment){output=std::move(result);return true;}
    if (progress) progress("Aligning Japanese phonemes with TIFA (native ONNX)");
    ++result.alignmentCallCount;
    if(stageDiagnostics)stageDiagnostics(result);
    if (!backend::TifaJapaneseAligner::align(audio, result.sequence.phones, result.sequence.groups,
        tifaRoot, execution, result.alignment, error, cancel))
    {if(recognitionReady)recognitionReady(result);return false;}
    result.alignmentBackend = result.alignment.activeBackend;
    result.alignmentModelFingerprint = result.alignment.modelFingerprint;
    if(recognitionReady)recognitionReady(result); // stage diagnostics, still not publishable
    if(result.alignment.phones.size()!=static_cast<std::size_t>(result.sequence.phones.size()))
    {error="TIFA phone count does not match the requested sequence";return false;}
    if (result.alignment.skippedPhones)
    { error = "TIFA skipped " + juce::String(result.alignment.skippedPhones) + " phonemes; correct/review the transcript instead of publishing fabricated boundaries"; return false; }
    if (dictionaryHash != juce::SHA256(dictionary).toHexString())
    { error = "Japanese dictionary changed during annotation"; return false; }
    result.audioId = "ja_" + digest(file.getFullPathName() + "|" + audio.fingerprint + "|"
        + inputTokens.joinIntoString(" ") + "|" + result.alignmentModelFingerprint + "|" + dictionaryHash).substring(0, 28);
    for (std::size_t i = 0; i < result.sequence.syllables.size(); ++i)
    {
        const auto& group = result.sequence.syllables[i];
        const auto& first = result.alignment.phones.at(std::size_t(group.firstPhone));
        const auto& last = result.alignment.phones.at(std::size_t(group.firstPhone + group.phoneCount - 1));
        SampleRegionSetting row; row.name = group.alias;
        row.materialAudioId = result.audioId; row.materialRegionId = "ja_r" + juce::String(int(i + 1));
        row.regionStartSeconds = first.startSeconds; row.regionEndSeconds = last.endSeconds;
        if (!(row.regionEndSeconds > row.regionStartSeconds) || row.regionStartSeconds < 0 || row.regionEndSeconds > audio.duration + 1.e-8)
        { error = "TIFA returned an empty or invalid pronunciation interval"; return false; }
        // TextGrid2oto ds_json2word uses the first phone's offset as `middle`
        // for multi-phone groups, and xmin for a single phone. Preserve native
        // full syllable coverage (no UTAU CVV left/right cropping).
        const auto middle = group.vowelPhone < 0 ? row.regionEndSeconds
            : group.phoneCount == 1 ? row.regionStartSeconds : first.endSeconds;
        row.alignmentSeconds = middle;
        // CV reference: fixed = prevoice + vowelDuration/3, cross=prevoice/2.
        // A closure-only transition stays entirely protected/non-elastic.
        row.fixedDurationSeconds = middle - row.regionStartSeconds
            + (group.vowelPhone < 0 ? 0.0 : (row.regionEndSeconds - middle) / 3.0);
        row.overlapSeconds = group.vowelPhone < 0 ? 0.0
            : group.phoneCount == 1 ? juce::jmin(.020, (row.regionEndSeconds - row.regionStartSeconds) * .5)
            : (middle - row.regionStartSeconds) * .5;
        row.provenance = "japanese-asr-tifa-textgrid2oto-native-v2"; row.confidence = 0;
        row.simplifiedConsonant = false; row.vowelSplitsExplicit = false;
        row.pronunciationKind = group.vowelPhone < 0 ? PronunciationKind::transition : PronunciationKind::normal;
        row.role = row.fixedDurationSeconds > 1.e-9 ? NativeSegmentRole::consonant : NativeSegmentRole::vowel;
        // Build one canonical C/V definition via the existing native authority.
        if (!applyPronunciationTiming(row, pronunciationTimingFor(row), error)) return false;
        NativePronunciationEdges edges;
        if(!defaultNativePronunciationEdges(row,edges,error))return false;
        edges.provenance="textgrid2oto-cvvc-native-exposed-v1";
        row.nativePronunciationEdges=std::move(edges);
        result.rows.push_back(std::move(row));
    }
    // Actual consecutive aligned syllables in this recording explicitly retain
    // their source seam. Selection may omit neighbours; edge planning then
    // exposes the chosen substring rather than fading every internal syllable.
    for(std::size_t i=1;i<result.rows.size();++i)
        if(std::abs(result.rows[i-1].regionEndSeconds-result.rows[i].regionStartSeconds)<=1.e-8)
            result.rows[i-1].connectedNextRegionId=result.rows[i].materialRegionId;
    OttoMaterialIO::Document validation;
    validation.audio.push_back({result.audioId, file.getFileName(), result.rows});
    if (!OttoMaterialIO::validate(validation, error)) return false;
    if (cancel && cancel()) { error = "Annotation cancelled"; return false; }
    if (juce::SHA256(file).toHexString() != audio.fingerprint)
    { error = "Recording changed while annotation was running"; return false; }
    result.warnings.add("Japanese-only syllable annotation: TextGrid2oto middle/Prevoice and CV fixed/overlap rules, preserving full native source ranges. Review both exposed source edges in the shared editor. Raw TIFA cosine scores are not probabilities.");
    result.ready=true;
    output = std::move(result); return true;
}
}
