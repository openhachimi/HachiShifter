#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include "../NativeVirtualGroupIO.h"
#include <iostream>
namespace hachi
{
inline bool runNativeSplitTransitionMixerSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int i=0;i<3;++i)
    {
        const auto file=output.getChildFile(i==0?"left.wav":i==1?"bridge.wav":"right.wav");
        juce::AudioBuffer<float> pcm(1,144000);for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,.09f*std::sin(juce::MathConstants<double>::twoPi*(173+i*61)*s/48000.));
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    const std::array<juce::String,4> ids{"left","front","back","right"};
    for(int i=0;i<4;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=output.getChildFile(i==0?"left.wav":i==3?"right.wav":"bridge.wav");
        clip.startSeconds=i==0?0.:i==1?.8:i==2?1.1:1.;clip.durationSeconds=clip.sourceDurationSeconds=i==0?1.2:i==3?1.:.3;clip.sourceOffsetSeconds=i==1?1.:i==2?1.3:0.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        NoteData note;note.id=ids[std::size_t(i)];note.label="any-alias";note.durationSeconds=clip.durationSeconds;note.nativeUnpitched=true;clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};NativeSplitTransitionEvidence e;e.logicalId="one-logical-bridge";e.frontNoteId="front";e.backNoteId="back";e.sourceObjectIdentity=e.frontSourceObjectIdentity=e.backSourceObjectIdentity="source-graph-17";
    e.precedingElementIdentity=e.frontElementIdentity="element-21";e.followingElementIdentity=e.backElementIdentity="element-22";e.joinsPitches=e.joinsAmplitudes=e.joinsPhases=e.transitionIdentified=true;
    ProjectModel model;model.resetDocument(seed);const auto before=model.contentFingerprint();
    const auto ordered=std::vector<juce::String>{"left","front","back","right"};
    const auto group=model.createNativeVirtualConnection(ordered,false,error,.1,false,&e);
    check("four_physical_notes_commit_through_same_virtual_authority",group.isNotEmpty()&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeConnections.empty());if(group.isEmpty())return false;
    const auto committed=model.snapshot();model.undo();check("one_undo_restores_exact_pre_group_project",model.contentFingerprint()==before);model.redo();
    check("redo_keeps_one_logical_bridge_and_four_original_notes",model.snapshot().nativeVirtualGroups[0].splitTransition&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ordered&&model.snapshot().tracks[0].clips.size()==4);
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    check("production_render_resolves_three_roles_for_four_physical_slices",prepareNativeVirtualRender(model.snapshot(),projected,groups,error)&&groups.size()==1&&groups[0].plan.members.size()==4&&groups[0].plan.contributionMembers.size()==3&&groups[0].plan.handovers.size()==2);
    if(groups.empty())return false;
    const auto render=[&](const ProjectData& data,juce::AudioBuffer<float>& result,double seconds=2.)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
        if(const auto p=engine.renderProgress();p&&*p<.999){error="Split fixture render timeout";return false;}
        if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}
        const auto count=juce::roundToInt(seconds*48000.);result.setSize(2,count);result.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<count;offset+=257)engine.getNextAudioBlock({&result,offset,std::min(257,count-offset)});
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    std::array<juce::AudioBuffer<float>,4> solo;bool ready=true;
    for(int i=0;i<4;++i){auto one=seed;one.tracks[0].clips={track.clips[std::size_t(i)]};ready&=render(one,solo[std::size_t(i)]);}
    juce::AudioBuffer<float> actual;ready&=render(model.snapshot(),actual);double maximum=0;bool seamUnique=true;
    for(int s=0;ready&&s<96000;++s)
    {
        const auto weights=nativeVirtualContributions(groups[0].plan,s/48000.);seamUnique&=!(weights[1]>1.e-9&&weights[2]>1.e-9);
        for(int ch=0;ch<2;++ch){double expected=0;for(int i=0;i<4;++i)expected+=weights[std::size_t(i)]*solo[std::size_t(i)].getSample(ch,s);maximum=std::max(maximum,std::abs(actual.getSample(ch,s)-expected));}
    }
    std::cout<<"maximum_four_slice_PCM_difference="<<maximum<<std::endl;
    check("actual_four_source_audio_callback_matches_one_logical_bridge",ready&&maximum<2.e-6);
    check("bridge_halves_never_play_as_two_independent_full_bridges",ready&&seamUnique);
    const auto file=output.getChildFile("split-virtual.hjpx");ProjectModel cold;
    check("four_slice_bridge_evidence_and_direction_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups.size()==1&&cold.snapshot().nativeVirtualGroups[0].splitTransition&&cold.snapshot().nativeVirtualGroups[0].splitTransition->sourceObjectIdentity==e.sourceObjectIdentity&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==ordered);
    check("cold_group_reenables_via_same_existing_group_adapter",cold.setNativeVirtualConnectionEnabled(group,false,error)&&cold.setNativeVirtualConnectionForNotes(ordered,true,false,error)&&cold.snapshot().nativeVirtualGroups.size()==1);
    juce::AudioBuffer<float> raw,off;const auto rawReady=render(seed,raw);model.setNativeVirtualConnectionEnabled(group,false,error);bool same=rawReady&&render(model.snapshot(),off);
    for(int ch=0;same&&ch<2;++ch)same&=std::equal(raw.getReadPointer(ch),raw.getReadPointer(ch)+96000,off.getReadPointer(ch));
    check("disabled_four_slice_group_restores_original_PCM_bit_exact",same);
    NativeVirtualGroup decoded;auto tree=nativeVirtualGroupTree(committed.nativeVirtualGroups[0]);check("four_member_schema_decodes_with_exact_evidence",nativeVirtualGroupFromTree(tree,decoded,error)&&decoded.splitTransition&&decoded.orderedNoteIds==ordered);
    for(auto child:tree)if(child.hasType("SplitTransitionEvidence"))child.setProperty("backSourceObjectIdentity","wrong-source",nullptr);
    check("corrupt_split_bridge_source_evidence_is_refused",!nativeVirtualGroupFromTree(tree,decoded,error));
    ProjectModel noProof;noProof.resetDocument(seed);const auto unchanged=noProof.contentFingerprint();check("arbitrary_four_voice_selection_cannot_bypass_bridge_proof",noProof.createNativeVirtualConnection(ordered,false,error).isEmpty()&&noProof.contentFingerprint()==unchanged);
    check("dissolve_restores_same_original_sources",model.removeNativeVirtualConnection(group,error)&&model.contentFingerprint()==before);
    ProjectModel common;common.resetDocument(seed);
    check("new_common_adapter_keeps_logical_bridge_order_despite_crossed_start_times",common.setNativeVirtualConnectionForNotes({"right","back","left","front"},true,false,error,.1,true,&e)&&common.snapshot().nativeVirtualGroups[0].orderedNoteIds==ordered&&common.snapshot().nativeVirtualGroups[0].automaticallyCreated&&common.snapshot().nativeConnections.empty());
    int movedCases=0;bool bound=true,renderable=true,partition=true,unique=true,pcmExact=true;
    for(const auto& [owner,position]:std::vector<std::pair<juce::String,double>>{{"owner-front",1.5},{"owner-back",1.6},{"owner-back",.7},{"owner-right",3.},{"owner-left",2.5}})
    {
        ProjectModel moved;moved.resetDocument(committed);moved.moveClip(owner,position);const auto state=moved.snapshot();
        bound&=state.nativeVirtualGroups.size()==1&&state.nativeVirtualGroups[0].id==group&&state.nativeVirtualGroups[0].orderedNoteIds==ordered;
        ProjectData local;std::vector<NativeVirtualRenderGroup> resolved;
        if(!prepareNativeVirtualRender(state,local,resolved,error)||resolved.size()!=1){renderable=false;continue;}
        std::array<juce::AudioBuffer<float>,4> stems;juce::AudioBuffer<float> mixed;bool live=render(state,mixed,4.5);
        for(int i=0;i<4;++i){auto one=state;one.nativeVirtualGroups.clear();one.tracks[0].clips={state.tracks[0].clips[std::size_t(i)]};live&=render(one,stems[std::size_t(i)],4.5);}
        if(!live){pcmExact=false;continue;}
        double caseMaximum=0;int caseSample=0;
        for(int sample=0;sample<216000;++sample)
        {
            const auto time=sample/48000.;const auto w=nativeVirtualContributions(resolved[0].plan,time);bool active=false;
            for(const auto& member:resolved[0].plan.members)active|=time>=member.from&&time<member.to;
            partition&=std::abs(w[0]+w[1]+w[2]+w[3]-(active?1.:0.))<1.e-8;
            unique&=!(w[1]>1.e-9&&w[2]>1.e-9);
            for(int ch=0;ch<2;++ch){double expected=0;for(int i=0;i<4;++i)expected+=w[std::size_t(i)]*stems[std::size_t(i)].getSample(ch,sample);const auto difference=std::abs(mixed.getSample(ch,sample)-expected);if(difference>caseMaximum){caseMaximum=difference;caseSample=sample;}pcmExact&=difference<2.e-6;}
        }
        std::cout<<"moved_owner="<<owner<<" position="<<position<<" max_difference="<<caseMaximum<<" sample="<<caseSample<<std::endl;
        const auto currentId=state.nativeVirtualGroups[0].id;
        bound&=moved.setNativeVirtualConnectionEnabled(currentId,false,error)&&moved.setNativeVirtualConnectionEnabled(currentId,true,error)&&moved.snapshot().nativeVirtualGroups[0].orderedNoteIds==ordered;
        ++movedCases;
    }
    check("moving_each_split_half_or_outer_endpoint_never_reorders_binding",bound&&movedCases==5);
    check("separated_overlapping_reversed_split_halves_remain_renderable",renderable&&movedCases==5);
    check("moved_group_partitions_only_real_available_physical_support",partition&&movedCases==5);
    check("moved_halves_never_become_two_independent_bridge_voices",unique&&movedCases==5);
    check("all_moved_cases_real_PCM_matches_single_group_route",pcmExact&&movedCases==5);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
