#pragma once
#include "JapaneseAutoAnnotation.h"
#include "ProjectMaterialRegistrationFacts.h"
#include "NativeVirtualGroupPlan.h"
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <map>
#include <set>
#include <thread>
namespace hachi
{
// Pure note-slice recognition adapter. The one registration lease remains the
// only publisher: this class never writes source audio, OTO or project state.
class MelodyneJapaneseSliceBatch final
{
public:
    struct Options
    {
        juce::File modelRoot;
        backend::OrtExecutionConfig execution;
        int workers=2;
        // Diagnostic seam: the production path always uses JapanesePhonemeAsr.
        std::function<bool(const backend::AnnotationAudio&,backend::JapaneseAsrResult&,juce::String&,const backend::AnnotationCancel&)> recognizeOverride;
    };
    struct Result
    {
        std::vector<ProjectMaterialRegistration::VariantFacts> variants;
        juce::String report;
        std::map<juce::String,juce::String> sourceFingerprints;
        int taskCount=0,asrCalls=0,reused=0,skipped=0;
        bool ready=false;
    };
    static bool recognize(const ProjectData& project,const std::vector<ProjectMaterialRegistration::VariantFacts>& facts,
        const Options& options,Result& output,juce::String& error,const backend::AnnotationCancel& cancel={},
        const std::function<void(const juce::String&)>& progress={})
    {
        error.clear();Result result;result.variants=facts;
        const auto cancelled=[&]{return cancel&&cancel();};
        const auto fail=[&](const juce::String& why){error=why;return false;};
        if(cancelled())return fail("Annotation cancelled");
        const auto modelRoot=options.modelRoot==juce::File{}?JapaneseAutoAnnotation::defaultModelRoot():options.modelRoot;
        const auto dictionary=modelRoot.getChildFile("tifa/japanese_dict_full.txt");
        const std::array<juce::File,4> resources{modelRoot.getChildFile("asr/model.onnx"),modelRoot.getChildFile("asr/phoneme_vocab.json"),modelRoot.getChildFile("asr/model.meta.json"),dictionary};
        std::array<juce::String,4> hashes;
        for(std::size_t i=0;i<resources.size();++i){if(!resources[i].existsAsFile())return fail("Japanese recognition resource is missing: "+resources[i].getFileName());hashes[i]=juce::SHA256(resources[i]).toHexString();}
        juce::String version="melodyne-note-syllables-v2";for(const auto& hash:hashes)version+="|"+hash;
        const auto digest=[](const juce::String& value){const auto bytes=value.toUTF8();return juce::SHA256(bytes.getAddress(),std::size_t(bytes.sizeInBytes()-1)).toHexString();};
        std::set<juce::String> bridgeNotes;
        for(const auto& annotation:project.logicalTransitions){bridgeNotes.insert(annotation.evidence.frontNoteId);bridgeNotes.insert(annotation.evidence.backNoteId);}
        for(const auto& group:project.nativeVirtualGroups)for(const auto* proof:nativeVirtualSplitProofs(group)){bridgeNotes.insert(proof->frontNoteId);bridgeNotes.insert(proof->backNoteId);}
        struct Source{double rate=0;juce::int64 length=0;juce::String hash;};std::map<juce::String,Source> sources;
        struct Destination{std::size_t variant,row;};
        struct Task{juce::File file;juce::int64 first=0,last=0;juce::String key;std::vector<Destination> destinations;};
        std::vector<Task> tasks;std::map<juce::String,std::size_t> indices;
        for(std::size_t v=0;v<facts.size();++v)for(std::size_t r=0;r<facts[v].rows.size();++r)
        {
            if(cancelled())return fail("Annotation cancelled");
            const auto& carried=facts[v].rows[r];const auto& row=carried.annotation;
            // Existing authored aliases and explicit bridges are never renamed.
            bool bridge=row.pronunciationKind==PronunciationKind::transition;
            for(const auto& member:carried.members)bridge|=bridgeNotes.count(member.noteId)>0;
            if(bridge){++result.skipped;continue;}
            if(!row.name.isEmpty()&&row.name!="-"){result.variants[v].rows[r].publicationAlias=row.name;++result.skipped;continue;}
            const auto file=facts[v].recording;const auto path=file.getFullPathName();
            auto found=sources.find(path);
            if(found==sources.end())
            {
                juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if(!reader||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>384000||reader->lengthInSamples<1)return fail("Cannot open source recording for Japanese note recognition: "+file.getFileName());
                found=sources.emplace(path,Source{reader->sampleRate,reader->lengthInSamples,juce::SHA256(file).toHexString()}).first;
            }
            const auto& source=found->second;
            if(!std::isfinite(row.regionStartSeconds)||!std::isfinite(row.regionEndSeconds)||row.regionEndSeconds<=row.regionStartSeconds)return fail("Invalid carried pronunciation interval for Japanese recognition");
            const auto first=juce::int64(std::llround(row.regionStartSeconds*source.rate)),last=juce::int64(std::llround(row.regionEndSeconds*source.rate));
            if(first<0||last<=first||last>source.length)return fail("Japanese note slice lies outside its real source recording");
            if((last-first)/source.rate>60.){++result.skipped;result.report+="Skipped overlong note slice: "+file.getFileName()+"\n";continue;}
            juce::String identity=path.replace("\\","/").toLowerCase()+"|"+source.hash+"|"+juce::String(first)+":"+juce::String(last)+"|P="+juce::String(juce::int64(std::llround(row.alignmentSeconds*source.rate)))+"|C="+juce::String(juce::int64(std::llround((row.regionStartSeconds+row.fixedDurationSeconds)*source.rate)))+"|"+version;
            // Time-map provenance is normalized to this source interval and note
            // origin, never to project placement/IDs. Reused b remains reusable.
            std::set<juce::String> maps;
            for(const auto& member:carried.members)for(const auto& track:project.tracks)if(track.id==member.trackId)for(const auto& parent:track.clips)if(parent.id==member.parentClipId)
            {
                const ClipData* owner=member.sourcePartId.isEmpty()?&parent:nullptr;for(const auto& part:parent.parts)if(part.id==member.sourcePartId)owner=&part;
                const NoteData* note=nullptr;for(const auto& n:parent.notes)if(n.id==member.noteId)note=&n;
                if(!owner||!note)return fail("Japanese note slice lost its exact physical owner");
                const auto clock=nativeClipClock(*owner);const auto offset=parent.parts.empty()?0.:owner->startSeconds;
                juce::String stamp;for(const auto& point:clock)stamp+=juce::String(point.targetSeconds-(note->startSeconds-offset),9)+","+juce::String(owner->sourceOffsetSeconds+point.sourceSeconds-row.regionStartSeconds,9)+";";
                maps.insert(stamp);
            }
            // Rendering time maps are occurrence provenance, not ASR audio:
            // the same source b used inside a/b and b/c is decoded identically.
            // Keep/validate maps above, but do not duplicate recognition because
            // the target placement or unrelated owner span differs.
            juce::ignoreUnused(maps);
            const auto key=digest(identity);const auto old=indices.find(key);
            if(old==indices.end()){indices[key]=tasks.size();tasks.push_back({file,first,last,key,{{v,r}}});}
            else{tasks[old->second].destinations.push_back({v,r});++result.reused;}
        }
        for(const auto& [path,source]:sources)result.sourceFingerprints[path]=source.hash;
        result.taskCount=int(tasks.size());
        struct Outcome{juce::String alias,raw,error;bool called=false,fatal=false;};std::vector<Outcome> outcomes(tasks.size());
        std::atomic_size_t next{0};std::atomic_bool fatal{false};
        const auto stop=[&]{return fatal.load()||cancelled();};
        const auto work=[&]
        {
            for(;;)
            {
                const auto index=next.fetch_add(1);if(index>=tasks.size()||stop())return;const auto& task=tasks[index];auto& outcome=outcomes[index];
                try
                {
                    backend::AnnotationAudio audio;
                    if(!backend::AnnotationAudio::loadSlice(task.file,task.first,task.last,audio,outcome.error,stop))
                    {outcome.fatal=true;fatal.store(true);return;}
                    backend::JapaneseAsrResult recognized;outcome.called=true;
                    const bool success=options.recognizeOverride?options.recognizeOverride(audio,recognized,outcome.error,stop):backend::JapanesePhonemeAsr::transcribe(audio,modelRoot.getChildFile("asr"),options.execution,recognized,outcome.error,stop);
                    if(!success)
                    {
                        // An empty/unknown recognition is an unresolved slice,
                        // not a language diagnosis or a confidence rejection.
                        if(outcome.error.contains("No Japanese phonemes")||outcome.error.contains("unknown phone"))continue;
                        if(stop())return;outcome.fatal=true;fatal.store(true);return;
                    }
                    outcome.raw=recognized.tokens.joinIntoString(" ");JapaneseAutoAnnotation::PhoneSequence sequence;
                    if(!JapaneseAutoAnnotation::phonemesToSyllables(recognized.tokens,dictionary,sequence,outcome.error))continue;
                    juce::StringArray syllables;for(const auto& syllable:sequence.syllables)syllables.add(syllable.alias);
                    // A note may carry more than one mora; preserve the explicit
                    // dictionary syllable sequence, not raw consonant/vowel phones.
                    outcome.alias=syllables.joinIntoString(" ");
                    if(progress)progress(juce::String(index+1)+"/"+juce::String(tasks.size())+" "+task.file.getFileName());
                }
                catch(const std::exception& exception){outcome.error=juce::String::fromUTF8(exception.what());outcome.fatal=true;fatal.store(true);return;}
            }
        };
        std::vector<std::thread> threads;const auto workers=std::min<std::size_t>(std::size_t(std::clamp(options.workers,1,4)),tasks.size());
        try{for(std::size_t i=0;i<workers;++i)threads.emplace_back(work);}catch(...){fatal.store(true);for(auto& thread:threads)thread.join();return fail("Could not start Japanese recognition workers");}
        for(auto& thread:threads)thread.join();
        if(cancelled())return fail("Annotation cancelled");
        for(const auto& outcome:outcomes)if(outcome.fatal)return fail(outcome.error.isEmpty()?juce::String("Japanese recognition failed"):outcome.error);
        for(const auto& [path,source]:sources)if(juce::SHA256(juce::File(path)).toHexString()!=source.hash)return fail("Source recording changed during Japanese recognition");
        for(std::size_t i=0;i<resources.size();++i)if(juce::SHA256(resources[i]).toHexString()!=hashes[i])return fail("Japanese model or dictionary changed during recognition");
        for(std::size_t i=0;i<tasks.size();++i)
        {
            const auto& task=tasks[i];const auto& outcome=outcomes[i];result.asrCalls+=int(outcome.called);
            if(outcome.alias.isEmpty()){result.skipped+=int(task.destinations.size());result.report+="Unresolved note slice "+task.file.getFileName()+" ["+juce::String(task.first)+","+juce::String(task.last)+"): "+outcome.error+"\n";continue;}
            for(const auto& destination:task.destinations){auto& row=result.variants[destination.variant].rows[destination.row];row.annotation.name=outcome.alias;row.publicationAlias=outcome.alias;}
            result.report+=task.file.getFileName()+" ["+juce::String(task.first)+","+juce::String(task.last)+") phones="+outcome.raw+"; syllables="+outcome.alias+"; occurrences="+juce::String(task.destinations.size())+"\n";
        }
        if(!tasks.empty()&&std::none_of(outcomes.begin(),outcomes.end(),[](const auto& item){return item.alias.isNotEmpty();}))
            return fail("No note slice produced dictionary-normalized Japanese syllables; no automatic annotations were published");
        result.report+="Unique slices="+juce::String(result.taskCount)+"; ASR calls="+juce::String(result.asrCalls)+"; reused="+juce::String(result.reused)+"; skipped="+juce::String(result.skipped)+"\n";
        result.ready=true;output=std::move(result);return true;
    }
    // Standalone diagnostic uses the SAME copied-material registration lease,
    // never writes an OTO beside the user's original source recordings.
    static bool run(const ProjectData& project,const juce::File& output,const juce::File& modelRoot,
        const backend::OrtExecutionConfig& execution,juce::String& report,const backend::AnnotationCancel& cancel={})
    {
        juce::String error;const auto facts=ProjectMaterialRegistrationFacts::derive(project,ProjectMaterialRegistrationFacts::Policy::ConfirmedCarriedPlacements);
        if(!facts.ready){report=facts.diagnostics.joinIntoString("\n");return false;}
        Options options;options.modelRoot=modelRoot;options.execution=execution;Result result;
        if(!recognize(project,facts.variants,options,result,error,cancel)){report=error;return false;}
        auto plan=ProjectMaterialRegistration::prepare(project,0,output,"recognized-materials-"+juce::Uuid().toString().substring(0,8),result.variants,ProjectModel::registrationMusicSourceFingerprint,cancel);
        if(!plan.ready){report=result.report+plan.diagnostics.joinIntoString("\n");return false;}
        auto lease=ProjectMaterialRegistration::stage(plan,error,cancel);
        if(!lease||!lease->publish(error,cancel)||!lease->release(error)){report=result.report+error;return false;}
        report=result.report+"Published: "+plan.publishedFolder.getFullPathName()+"\n";
        OttoMaterialIO::Document cold;if(!OttoMaterialIO::loadDocument(plan.publishedFolder.getChildFile("otto.ini"),cold,error)){report+=error;return false;}
        int expected=0,actual=0;for(const auto& audio:plan.document.audio)for(const auto& row:audio.rows)expected+=row.name.isNotEmpty()&&row.name!="-";
        for(const auto& audio:cold.audio)for(const auto& row:audio.rows)actual+=row.name.isNotEmpty()&&row.name!="-";
        report+="Cold-read syllable rows="+juce::String(actual)+"; planned="+juce::String(expected)+"\n";
        return expected>0&&actual==expected;
    }
};
}
