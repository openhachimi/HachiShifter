#pragma once
#include "../backend/MelodyneImporter.h"
#include "../backend/NsfHifiganRenderer.h"
#include "../MelodyneImportOptimization.h"
#include "../NativeVirtualRenderPlan.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
// Own output files only. No original-sidecar conversion/publication, no media
// duplication, no whole-song inference and no claim of listening acceptance.
inline bool runNativeRealMultiSplitAudioSmoke(const juce::File& source,const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    backend::MelodyneImportOptions options;options.recursiveMediaSearch=false;
    auto imported=backend::MelodyneImporter::importProject(source,error,{},options);
    check("actual_project_imported_with_all_source_recordings",imported&&imported->missingFiles.isEmpty());if(!imported)return false;
    const auto candidates=detectMelodyneSplitTransitions(imported->project,imported->continuousSourceJoins);
    const NativeSplitTransitionCandidate* first=nullptr;const NativeSplitTransitionCandidate* second=nullptr;
    for(const auto& a:candidates)for(const auto& b:candidates)if(a.evidence.logicalId!=b.evidence.logicalId&&a.orderedNoteIds.back()==b.orderedNoteIds.front()){first=&a;second=&b;}
    check("two_proven_real_bridges_share_one_normal_endpoint",first&&second);if(!first||!second)return false;
    const auto firstProof=first->evidence,secondProof=second->evidence;
    std::vector<juce::String> ids=first->orderedNoteIds;ids.insert(ids.end(),second->orderedNoteIds.begin()+1,second->orderedNoteIds.end());
    std::set<juce::String> selected(ids.begin(),ids.end());ProjectData raw=imported->project;raw.tracks.clear();raw.nativeConnections.clear();raw.nativeVirtualGroups.clear();raw.logicalTransitions.clear();
    double earliest=std::numeric_limits<double>::infinity();
    for(const auto& t:imported->project.tracks)
    {
        TrackData track=t;track.clips.clear();
        for(const auto& c:t.clips)
        {
            auto clip=c;std::erase_if(clip.notes,[&](const auto& note){return !selected.count(note.id);});
            if(clip.notes.empty())continue;
            if(!clip.parts.empty()){error="This diagnostic expects actual independent imported physical owners";check("scoped_physical_owners_are_independent",false);return false;}
            earliest=std::min(earliest,clip.startSeconds);track.clips.push_back(std::move(clip));
        }
        if(!track.clips.empty())raw.tracks.push_back(std::move(track));
    }
    check("only_the_seven_selected_physical_slices_are_kept",raw.tracks.size()==1&&raw.tracks[0].clips.size()==7&&selected.size()==7);if(raw.tracks.size()!=1)return false;
    log+="backend=WORLD diagnostic explicitly selected; NSF empty-config available="+juce::String(backend::NsfHifiganRenderer::modelAvailable({})?"true":"false")+"; not a global model-discovery result\n";
    for(auto& clip:raw.tracks[0].clips)clip.startSeconds+=.2-earliest;
    // Project-only conversion is the exact service used by the import GUI/MCP.
    // It must not publish an otto.ini next to the user's original recordings.
    annotateMelodyneSplitTransitions(raw,{*first,*second});juce::StringArray warnings;
    SampleSettings::convertMelodyneProject(raw,warnings,true,false);
    raw.tracks[0].pitchAlgorithm=PitchAlgorithm::world;raw.tracks[0].normalizeVolume=false;raw.tracks[0].smoothOverlaps=false;
    ProjectModel model;model.resetDocument(raw);
    const auto a=model.setNativeVirtualConnectionForNotes(first->orderedNoteIds,true,false,error,.1,false,&firstProof);
    const auto b=model.setNativeVirtualConnectionForNotes(second->orderedNoteIds,true,false,error,.1,false,&secondProof);
    check("real_converted_bridges_commit_one_group_without_compound_or_clock_rewrite",a&&b&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids&&model.snapshot().tracks[0].clips.size()==7);
    if(model.snapshot().nativeVirtualGroups.size()!=1)return false;
    const auto routed=model.snapshot();const auto groupId=routed.nativeVirtualGroups[0].id;
    ProjectModel beforeFacts,afterFacts;beforeFacts.resetDocument(raw);auto withoutGroups=routed;withoutGroups.nativeVirtualGroups.clear();afterFacts.resetDocument(withoutGroups);
    check("adding_real_bridges_preserves_every_original_authoring_source_fact",beforeFacts.contentFingerprint()==afterFacts.contentFingerprint());
    ProjectData projected;std::vector<NativeVirtualRenderGroup> plans;const bool resolved=prepareNativeVirtualRender(routed,projected,plans,error);
    check("real_group_has_five_logical_roles_not_seven_independent_voices",resolved&&plans.size()==1&&plans[0].plan.contributionMembers.size()==5&&plans[0].plan.handovers.size()==4);if(!resolved)return false;
    double duration=.2;for(const auto& c:raw.tracks[0].clips)duration=std::max(duration,c.startSeconds+c.durationSeconds+.15);
    const int count=int(std::ceil(duration*48000));log+="selected_source_timeline_from="+juce::String(earliest,9)+"; scoped_seconds="+juce::String(duration,9)+"\n";
    const auto render=[&](const ProjectData& state,const juce::String& name,juce::AudioBuffer<float>& played,bool save)->bool
    {
        const auto started=juce::Time::getMillisecondCounterHiRes();AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+25000.;while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(5);}
        if(const auto p=engine.renderProgress();p&&*p<.999){error="Scoped real-source render exceeded 25s";return false;}
        const auto diagnostic=engine.activeRenderWarnings();if(!diagnostic.isEmpty()){error=diagnostic;return false;}
        played.setSize(2,count);played.clear();engine.setPosition(0);engine.play();for(int start=0;start<count;start+=257)engine.getNextAudioBlock({&played,start,std::min(257,count-start)});engine.stop();
        log+=name+"_render_ms="+juce::String(juce::Time::getMillisecondCounterHiRes()-started,1)+" magnitude="+juce::String(played.getMagnitude(0,count),8)+"\n";
        if(save)
        {
            const auto file=output.getChildFile(name+".wav");if(!engine.exportWav(file,error))return false;
            juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));if(!reader)return false;
            juce::AudioBuffer<float> exported(2,int(std::min<juce::int64>(count,reader->lengthInSamples)));if(!reader->read(&exported,0,exported.getNumSamples(),0,true,true))return false;
            double difference=0;for(int ch=0;ch<2;++ch)for(int sample=0;sample<exported.getNumSamples();++sample)difference=std::max(difference,double(std::abs(played.getSample(ch,sample)-exported.getSample(ch,sample))));
            log+=name+"_playback_export_max_difference="+juce::String(difference,12)+"\n";if(difference>2.e-6){error="Actual scoped export disagrees with playback";return false;}
        }
        return true;
    };
    const auto exact=[&](const juce::AudioBuffer<float>& x,const juce::AudioBuffer<float>& y){if(x.getNumSamples()!=count||y.getNumSamples()!=count)return false;for(int ch=0;ch<2;++ch)if(!std::equal(x.getReadPointer(ch),x.getReadPointer(ch)+count,y.getReadPointer(ch)))return false;return true;};
    juce::AudioBuffer<float> baseline,route,off,coldAudio,glide;
    check("actual_scoped_world_baseline_and_route_render_and_export",render(raw,"A-original-no-virtual",baseline,true)&&render(routed,"B-virtual-route-no-glide",route,true)&&route.getMagnitude(0,count)>.001f);
    auto disabled=routed;disabled.nativeVirtualGroups[0].enabled=false;
    check("real_AB_disabled_route_is_bit_exact_to_no_virtual_group",render(disabled,"disabled",off,false)&&exact(baseline,off));
    const auto project=output.getChildFile("two-real-bridges.hjpx");ProjectModel cold;
    check("real_multi_bridge_cold_reload_retains_both_source_proofs",model.save(project,error)&&cold.load(project,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1);
    check("real_cold_playback_is_bit_exact_to_saved_route",render(cold.snapshot(),"cold",coldAudio,false)&&exact(route,coldAudio));
    auto pitched=routed;pitched.nativeVirtualGroups[0].automaticPitch=true;
    ProjectData pitchProjection;std::vector<NativeVirtualRenderGroup> pitchGroups;const bool pitchReady=prepareNativeVirtualRender(pitched,pitchProjection,pitchGroups,error);
    int activePitchWindows=0;if(pitchReady&&!pitchGroups.empty())for(const auto& segment:pitchGroups[0].pitch.segments)activePitchWindows+=segment.active?1:0;
    log+="active_actual_automatic_pitch_windows="+juce::String(activePitchWindows)+"\n";
    check("actual_pitch_optimization_does_not_mute_or_drop_source_proofs",pitchReady&&pitchGroups.size()==1&&pitchGroups[0].plan.members.size()==7&&render(pitched,"C-virtual-route-auto-glide",glide,true)&&glide.getMagnitude(0,count)>.001f);
    const auto difference=[&](const auto& x,const auto& y){double value=0;if(x.getNumSamples()!=count||y.getNumSamples()!=count)return value;for(int channel=0;channel<2;++channel)for(int sample=0;sample<count;++sample)value=std::max(value,double(std::abs(x.getSample(channel,sample)-y.getSample(channel,sample))));return value;};
    const auto routeChange=difference(baseline,route),glideChange=difference(route,glide);
    log+="real_route_PCM_change="+juce::String(routeChange,12)+"; real_glide_PCM_change="+juce::String(glideChange,12)+"\n";
    check("real_route_and_glide_actually_change_PCM_not_just_metadata",routeChange>1.e-6&&activePitchWindows==2&&glideChange>1.e-6);
    const auto rms=[&](const auto& buffer,double time){const int begin=std::max(0,int(std::round((time-.01)*48000))),end=std::min(count,int(std::round((time+.01)*48000)));double energy=0;for(int sample=begin;sample<end;++sample){const auto v=buffer.getSample(0,sample);energy+=v*v;}return end>begin?std::sqrt(energy/(end-begin)):0.;};
    if(route.getNumSamples()==count&&baseline.getNumSamples()==count&&glide.getNumSamples()==count)
        for(std::size_t i=0;i<plans[0].plan.handovers.size();++i)
        {
            const auto& handover=plans[0].plan.handovers[i];const auto middle=(handover.from+handover.to)*.5;
            log+="handover"+juce::String(int(i))+"_20ms_rms_original="+juce::String(rms(baseline,middle),9)+" route="+juce::String(rms(route,middle),9)+" glide="+juce::String(rms(glide,middle),9)+"\n";
        }
    // Don't call inactive/guarded automatic pitch an audible success.
    log+="pitch_plan_issue="+(pitchGroups.empty()?juce::String("missing"):pitchGroups[0].pitch.issue)+"\n";
    log+="listening_acceptance=NOT_PERFORMED; no NSF quality claim; actual WAVs are controlled WORLD A/B diagnostics\n";
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
