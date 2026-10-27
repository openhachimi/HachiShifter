#pragma once
#include "../backend/MelodyneImporter.h"
#include "../NativeCoupledSelectionMove.h"
#include "../SampleSettings.h"
#include "../AudioEngine.h"
#include "../backend/NsfHifiganRenderer.h"
#include <iostream>
namespace hachi
{
// Real imported project, real drag, real audio. Scoped to ONE actual partial
// cut and its own two owners so a bounded run stays honest about what it
// rendered; it is listening material, not an automatic quality verdict.
inline bool runNativeRealProjectPartialCutDrag(const juce::File& project,const juce::File& output,
    double dragSeconds,int semitones)
{
    if(output.createDirectory().failed())return false;
    juce::String error,log;bool ok=true;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    backend::MelodyneImportOptions options;options.recursiveMediaSearch=false;
    const auto before=juce::Time::getMillisecondCounterHiRes();
    auto imported=backend::MelodyneImporter::importProject(project,error,{},options);
    std::cout<<"import_ms="<<juce::roundToInt(juce::Time::getMillisecondCounterHiRes()-before)<<std::endl;
    check("real_melodyne_project_imported",imported&&!imported->project.tracks.empty());
    if(!imported)return false;
    juce::StringArray warnings;SampleSettings::convertMelodyneProject(imported->project,warnings,false,false);
    auto data=imported->project;
    // Find an ACTUAL imported partial cut: same usage identity, same recording,
    // reciprocal connection flags and a real coincident source/target seam.
    struct Pair{juce::String trackId,leftNote,rightNote,leftClip,rightClip;double seam=0;};
    std::optional<Pair> chosen;
    for(const auto& track:data.tracks)
    {
        if(!trackShowsAllNativeRegions(track)||!track.compose||track.accompaniment)continue;
        for(const auto& a:track.clips)for(const auto& b:track.clips)
        {
            if(chosen||&a==&b||a.notes.size()!=1||b.notes.size()!=1||!a.parts.empty()||!b.parts.empty())continue;
            if(a.sourceFile!=b.sourceFile||a.materialSourceUsageGroupId.isEmpty()
                ||a.materialSourceUsageGroupId!=b.materialSourceUsageGroupId)continue;
            if(!a.notes[0].connectedToNext||!b.notes[0].connectedToPrevious
                ||!a.glideConnectedToNext||!b.glideConnectedFromPrevious)continue;
            const auto ac=nativeClipClock(a),bc=nativeClipClock(b);if(ac.size()<2||bc.size()<2)continue;
            const auto aEnd=a.startSeconds+a.notes[0].startSeconds+a.notes[0].durationSeconds;
            const auto bStart=b.startSeconds+b.notes[0].startSeconds;
            const auto aSource=a.sourceOffsetSeconds+nativeSourceTimeAt(ac,a.notes[0].startSeconds+a.notes[0].durationSeconds);
            const auto bSource=b.sourceOffsetSeconds+nativeSourceTimeAt(bc,b.notes[0].startSeconds);
            if(std::abs(aEnd-bStart)>1.e-6||std::abs(aSource-bSource)>1.e-6)continue;
            chosen=Pair{track.id,a.notes[0].id,b.notes[0].id,a.id,b.id,aEnd};
        }
    }
    check("real_imported_partial_cut_located",chosen.has_value());
    if(!chosen)return false;
    std::cout<<"partial_cut_track="<<chosen->trackId<<" left="<<chosen->leftNote
        <<" right="<<chosen->rightNote<<" seam_seconds="<<chosen->seam<<std::endl;
    // Keep ONLY the two real owners on their real track so the bounded render
    // is the actual partial cut, with its original clocks and markers intact.
    ProjectData scoped;scoped.bpm=data.bpm;scoped.beatOriginSeconds=data.beatOriginSeconds;
    for(const auto& track:data.tracks)
    {
        if(track.id!=chosen->trackId)continue;auto copy=track;copy.clips.clear();
        for(const auto& clip:track.clips)if(clip.id==chosen->leftClip||clip.id==chosen->rightClip)copy.clips.push_back(clip);
        scoped.tracks.push_back(std::move(copy));
    }
    double origin=1.e9,finish=0;
    for(const auto& clip:scoped.tracks[0].clips){origin=std::min(origin,clip.startSeconds);finish=std::max(finish,clip.startSeconds+clip.durationSeconds);}
    for(auto& clip:scoped.tracks[0].clips)clip.startSeconds-=origin-.25;
    const auto span=finish-origin+1.;
    const auto seamOf=[&](const ProjectData& state,double& targetGap,double& sourceGap)
    {
        const ClipData *left=nullptr,*right=nullptr;
        for(const auto& clip:state.tracks[0].clips)
            for(const auto& note:clip.notes){if(note.id==chosen->leftNote)left=&clip;if(note.id==chosen->rightNote)right=&clip;}
        if(!left||!right)return false;
        const auto lc=nativeClipClock(*left),rc=nativeClipClock(*right);
        const auto& a=left->notes.front();const auto& b=right->notes.front();
        targetGap=(right->startSeconds+b.startSeconds)-(left->startSeconds+a.startSeconds+a.durationSeconds);
        sourceGap=(right->sourceOffsetSeconds+nativeSourceTimeAt(rc,b.startSeconds))
            -(left->sourceOffsetSeconds+nativeSourceTimeAt(lc,a.startSeconds+a.durationSeconds));
        return true;
    };
    double targetGap=1,sourceGap=1;
    check("scoped_real_pair_starts_joined",seamOf(scoped,targetGap,sourceGap)&&std::abs(targetGap)<1.e-6&&std::abs(sourceGap)<1.e-6);
    ProjectModel model;model.resetDocument(scoped);
    const auto originalFingerprint=model.contentFingerprint();
    const auto dragged=model.moveNativeNotes({chosen->rightNote},dragSeconds,float(semitones),&error);
    const auto after=model.snapshot();
    double movedTarget=1,movedSource=1;const auto measured=seamOf(after,movedTarget,movedSource);
    std::cout<<"drag_seconds="<<dragSeconds<<" semitones="<<semitones
        <<" target_gap="<<movedTarget<<" source_gap="<<movedSource<<std::endl;
    check("real_drag_keeps_the_imported_seam_joined",dragged&&measured&&std::abs(movedTarget)<1.e-6&&std::abs(movedSource)<1.e-6);
    check("real_drag_keeps_both_original_owners",after.tracks[0].clips.size()==scoped.tracks[0].clips.size());
    model.undo();check("real_drag_is_one_undoable_action",model.contentFingerprint()==originalFingerprint);model.redo();
    const auto render=[&](const ProjectData& state,const juce::File& file)
    {
        AudioEngine engine(false);engine.prepareToPlay(512,48000);engine.syncProject(state);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+120'000.0;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(20);}
        if(const auto p=engine.renderProgress();p&&*p<.999){error="Bounded render did not finish in 120 s";return false;}
        const auto warning=engine.activeRenderWarnings();if(warning.isNotEmpty()){error=warning;return false;}
        juce::String issue;if(!engine.exportWav(file,issue)){error=issue;return false;}
        return true;
    };
    juce::ignoreUnused(span);
    // Render with whatever engine this machine can actually run. NSF-HiFiGAN
    // needs a configured model pack; falling back is stated, never silent.
    {
        const bool neural=std::any_of(scoped.tracks.begin(),scoped.tracks.end(),
            [](const auto& t){return t.pitchAlgorithm==PitchAlgorithm::nsfHifigan;});
        if(neural&&!backend::NsfHifiganRenderer::modelAvailable({}))
        {
            for(auto& track:scoped.tracks)track.pitchAlgorithm=PitchAlgorithm::world;
            auto adjusted=model.snapshot();
            for(auto& track:adjusted.tracks)track.pitchAlgorithm=PitchAlgorithm::world;
            model.resetDocument(adjusted);
            log+="render_engine=WORLD (NSF-HiFiGAN model pack is not configured on this machine)\n";
            std::cout<<"render_engine=WORLD_fallback_model_pack_unavailable"<<std::endl;
        }
        else log+="render_engine=project default\n";
    }
    const auto beforeWav=output.getChildFile("A-original-partial-cut.wav");
    const auto afterWav=output.getChildFile("B-after-drag.wav");
    check("listening_sample_before_drag_rendered",render(scoped,beforeWav));
    check("listening_sample_after_drag_rendered",render(model.snapshot(),afterWav));
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    const auto describe=[&](const juce::File& file,const char* label)
    {
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if(!reader||reader->lengthInSamples<=0)return false;
        juce::AudioBuffer<float> pcm(int(reader->numChannels),int(std::min<juce::int64>(reader->lengthInSamples,48000*120)));
        if(!reader->read(&pcm,0,pcm.getNumSamples(),0,true,true))return false;
        double sum=0;float peak=0;
        for(int ch=0;ch<pcm.getNumChannels();++ch)for(int s=0;s<pcm.getNumSamples();++s)
        {const auto v=pcm.getSample(ch,s);sum+=double(v)*v;peak=std::max(peak,std::abs(v));}
        const auto rms=std::sqrt(sum/std::max(1,pcm.getNumSamples()*pcm.getNumChannels()));
        std::cout<<label<<"_seconds="<<reader->lengthInSamples/reader->sampleRate<<" rms="<<rms<<" peak="<<peak<<std::endl;
        log+=juce::String(label)+"_rms="+juce::String(rms,6)+" peak="+juce::String(peak,6)+"\n";
        return rms>1.e-5;
    };
    check("before_sample_contains_actual_audio",describe(beforeWav,"before"));
    check("after_sample_contains_actual_audio",describe(afterWav,"after"));
    output.getChildFile("verification.txt").replaceWithText(log);
    std::cout<<"listening_folder="<<output.getFullPathName()<<std::endl;
    return ok;
}
}
