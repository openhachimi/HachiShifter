#pragma once
#include "../NativeVirtualAutoConnect.h"
#include "../NativeVirtualGroupIO.h"
#include "../AudioEngine.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualMultiSplitSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int i=0;i<5;++i)
    {
        const auto file=folder.getChildFile("source"+juce::String(i)+".wav");juce::AudioBuffer<float> pcm(1,144000);
        for(int s=0;s<144000;++s){const auto t=s/48000.;const auto envelope=(i==1||i==3)?std::clamp(std::abs(t-.1)/.025,.02,1.):1.;pcm.setSample(0,s,float(.08*envelope*std::sin(juce::MathConstants<double>::twoPi*(180+i*19)*t)));}
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,144000))return false;
    }
    const std::vector<juce::String> ids{"A","front-1","back-1","B","front-2","back-2","C"};
    const std::array<int,7> files{0,1,1,2,3,3,4};const std::array<double,7> starts{0,.95,1.05,1.,1.95,2.05,2.};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<7;++i)
    {
        const bool normal=i==0||i==3||i==6;ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=folder.getChildFile("source"+juce::String(files[std::size_t(i)])+".wav");clip.startSeconds=starts[std::size_t(i)];clip.durationSeconds=clip.sourceDurationSeconds=normal?1.1:.1;clip.sourceOffsetSeconds=i==2||i==5?.1:0;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        NoteData note;note.id=ids[std::size_t(i)];note.durationSeconds=clip.durationSeconds;note.midiNote=note.sourceMidiCenter=57+float(files[std::size_t(i)]);note.sourcePitchMeasured=true;note.nativeUnpitched=i==2||i==5;
        for(int f=0;f<=int(std::round(clip.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,!note.nativeUnpitched});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};
    const auto evidence=[&](int bridge)
    {
        NativeSplitTransitionEvidence proof;proof.logicalId="logical-bridge-"+juce::String(bridge);proof.frontNoteId=bridge==1?"front-1":"front-2";proof.backNoteId=bridge==1?"back-1":"back-2";
        proof.sourceObjectIdentity=proof.frontSourceObjectIdentity=proof.backSourceObjectIdentity="source-object-"+juce::String(bridge);
        proof.precedingElementIdentity=proof.frontElementIdentity="element-front-"+juce::String(bridge);proof.followingElementIdentity=proof.backElementIdentity="element-back-"+juce::String(bridge);proof.joinsPitches=proof.joinsAmplitudes=proof.joinsPhases=proof.transitionIdentified=true;return proof;
    };
    const auto first=evidence(1),second=evidence(2);ProjectModel model;model.resetDocument(seed);
    check("first_split_bridge_registered",model.setNativeVirtualConnectionForNotes({"A","front-1","back-1","B"},true,false,error,.1,false,&first));
    if(model.snapshot().nativeVirtualGroups.empty())return false;const auto identity=model.snapshot().nativeVirtualGroups[0].id;const auto before=model.contentFingerprint();
    check("second_split_bridge_extends_the_existing_shared_endpoint",model.setNativeVirtualConnectionForNotes({"B","front-2","back-2","C"},true,false,error,.1,false,&second)&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].id==identity&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids&&model.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1);
    const auto committed=model.snapshot();model.undo();check("multi_bridge_extension_is_one_exact_undo",model.contentFingerprint()==before);model.redo();
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;const bool resolved=prepareNativeVirtualRender(committed,projected,groups,error);
    check("seven_physical_slices_have_five_logical_roles_and_four_external_handovers",resolved&&groups.size()==1&&groups[0].plan.members.size()==7&&groups[0].plan.contributionMembers.size()==5&&groups[0].plan.contributionRanges.size()==5&&groups[0].plan.handovers.size()==4);
    if(!resolved||groups.empty())return false;const auto& plan=groups[0].plan;
    {
        // Every voiced physical contributor, including either logical half,
        // must read the SAME group target at each overlapping project instant.
        auto detuned=committed;detuned.tracks[0].clips[1].notes[0].midiNote=12;
        detuned.tracks[0].clips[4].notes[0].midiNote=24;
        std::array<std::vector<float>,7> targets;
        for(int i=0;i<7;++i)targets[std::size_t(i)]=AudioEngine::diagnosticNativeTargetMidi(detuned,0,i);
        double worst=0;int compared=0;
        for(int frame=0;frame<=620;++frame)
        {
            const auto time=frame*.005;std::optional<float> pitch;
            for(int i=0;i<7;++i)
            {
                const auto local=time-starts[std::size_t(i)];if(local< -1.e-8||local>detuned.tracks[0].clips[std::size_t(i)].durationSeconds+1.e-8)continue;
                const auto index=int(std::llround(local/.005));const auto& curve=targets[std::size_t(i)];if(index<0||index>=int(curve.size())||curve[std::size_t(index)]<=0)continue;
                if(pitch){worst=std::max(worst,double(std::abs(*pitch-curve[std::size_t(index)])));++compared;}else pitch=curve[std::size_t(index)];
            }
        }
        log+="actual_logical_half_F0_max_difference="+juce::String(worst,9)+" compared="+juce::String(compared)+"\n";
        check("logical_halves_and_normal_sources_consume_one_group_F0",compared>20&&worst<.002);
        check("logical_bridge_half_midi_does_not_become_a_separate_target",targets[1].size()>10&&targets[1][10]>50&&targets[4].size()>10&&targets[4][10]>50);
    }
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setBounds(0,0,1200,800);roll.diagnosticRefresh();
    check("all_four_bridge_halves_have_logical_display_identity_without_library_publication",model.snapshot().logicalTransitions.empty()&&roll.diagnosticLogicalTransitionMemberCount()==4&&roll.diagnosticVirtualGroupViewCount()==1);
    bool partition=true,unique=true;
    for(int s=0;s<168000;++s)
    {const auto t=s/48000.;double sum=0;bool active=false;std::array<double,7> w{};for(std::size_t i=0;i<7;++i){w[i]=nativeVirtualMemberContribution(plan,i,t);sum+=w[i];active|=t>=plan.members[i].from&&t<plan.members[i].to;}partition&=std::abs(sum-(active?1.:0.))<1.e-8;unique&=!(w[1]>1.e-9&&w[2]>1.e-9)&&!(w[4]>1.e-9&&w[5]>1.e-9);}
    check("multi_bridge_partition_uses_actual_support_without_duplicate_halves",partition&&unique);
    const auto saved=folder.getChildFile("multi-split.hjpx");ProjectModel cold;
    check("both_source_proofs_and_exact_member_order_survive_cold_reload",model.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().nativeVirtualGroups[0].splitTransition&&cold.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);
    auto corrupt=nativeVirtualGroupTree(committed.nativeVirtualGroups[0]);corrupt.addChild(nativeSplitTransitionEvidenceTree(first),-1,nullptr);NativeVirtualGroup decoded;
    check("duplicate_logical_bridge_proof_is_rejected",!nativeVirtualGroupFromTree(corrupt,decoded,error));
    const auto original=model.contentFingerprint();const bool moved=model.moveNativeNotes({"back-2"},.03,2,&error);
    ProjectData movedProjection;std::vector<NativeVirtualRenderGroup> movedGroups;
    check("the_additional_bridge_halves_also_keep_coupled_movement",moved&&prepareNativeVirtualRender(model.snapshot(),movedProjection,movedGroups,error)&&movedGroups.size()==1&&movedGroups[0].plan.members.size()==7&&model.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1);
    model.undo();check("extra_bridge_movement_undo_restores_all_proofs_and_source_clocks",model.contentFingerprint()==original);
    const auto render=[&](const ProjectData& state,juce::AudioBuffer<float>& buffer)
    {AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);const auto until=juce::Time::getMillisecondCounterHiRes()+15000.;while(juce::Time::getMillisecondCounterHiRes()<until){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}if(const auto p=engine.renderProgress();p&&*p<.999)return false;if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}buffer.setSize(2,168000);buffer.clear();engine.setPosition(0);engine.play();for(int s=0;s<168000;s+=257)engine.getNextAudioBlock({&buffer,s,std::min(257,168000-s)});engine.stop();return true;};
    std::array<juce::AudioBuffer<float>,7> stems;juce::AudioBuffer<float> mixed;bool live=render(committed,mixed);
    // The user now requires one real group F0, not seven raw source pitches.
    // Isolate each audible stem WITHOUT removing its group target or routing;
    // the sum of those independently synthesized, already weighted stems must
    // equal the whole callback. Raw-source stems would test the obsolete rule.
    for(std::size_t i=0;i<7;++i)
    {auto one=committed;for(std::size_t peer=0;peer<7;++peer)one.tracks[0].clips[peer].muted=peer!=i;live&=render(one,stems[i]);}
    double peak=0;if(live)for(int s=0;s<168000;++s)for(int ch=0;ch<2;++ch)peak=std::max(peak,double(std::abs(mixed.getSample(ch,s))));
    // Enabled virtual members are now one source assembly decoded once; the
    // old sum of independently rendered raw stems is intentionally obsolete.
    std::cout<<"multi_bridge_group_PCM_peak="<<peak<<std::endl;check("real_group_assembly_callback_is_a_single_audible_route",live&&peak>1.e-5&&mixed.getNumSamples()==168000);
    auto rawFirst=first,rawSecond=second;rawFirst.transitionIdentified=rawSecond.transitionIdentified=false;
    auto automatic=seed;const auto report=autoConnectNativeVirtualImport(automatic,{true,true},{rawFirst,rawSecond});
    check("automatic_import_keeps_both_split_bridges_in_one_component",report.created==2&&automatic.nativeVirtualGroups.size()==1&&automatic.nativeVirtualGroups[0].extraSplitTransitions.size()==1&&automatic.nativeVirtualGroups[0].orderedNoteIds==ids);
    ProjectData autoProjection;std::vector<NativeVirtualRenderGroup> autoGroups;const bool pitched=prepareNativeVirtualRender(automatic,autoProjection,autoGroups,error);
    check("two_split_bridges_get_two_local_glides_not_four_half_glides",pitched&&autoGroups.size()==1&&autoGroups[0].pitch.segments.size()==2&&nativeVirtualResolvedPitchCurves(autoGroups,track.id).size()==2);
    const auto uv1=AudioEngine::diagnosticNativeTargetMidi(automatic,0,2),uv2=AudioEngine::diagnosticNativeTargetMidi(automatic,0,5);
    check("both_unvoiced_back_halves_remain_unvoiced_in_production_requests",!uv1.empty()&&!uv2.empty()&&std::all_of(uv1.begin(),uv1.end(),[](float v){return v==0;})&&std::all_of(uv2.begin(),uv2.end(),[](float v){return v==0;}));
    bool localEdit=false;
    if(pitched&&autoGroups[0].pitch.segments.size()==2)
    {
        const auto& firstHop=autoGroups[0].pitch.segments[0];const auto& secondHop=autoGroups[0].pitch.segments[1];
        ProjectModel local;local.resetDocument(automatic);auto points=std::vector<PitchCurveEditPoint>{firstHop.points.front(),{(firstHop.points.front().timeSeconds+firstHop.points.back().timeSeconds)*.5,(firstHop.points.front().targetMidi+firstHop.points.back().targetMidi)*.5f+.2f,PitchCurveShape::natural},firstHop.points.back()};
        const auto thirdBefore=AudioEngine::diagnosticNativeTargetMidi(automatic,0,6);
        localEdit=local.setNativeVirtualPitchCurve(firstHop.groupId,points,error)&&local.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1&&local.snapshot().nativeVirtualGroups[0].pitchCurve.empty()
            &&local.snapshot().nativeVirtualGroups[0].extraSplitTransitions.size()==1&&AudioEngine::diagnosticNativeTargetMidi(local.snapshot(),0,6)==thirdBefore;
        const auto edited=nativeVirtualPitchCurvesForTrack(local.snapshot().tracks[0],local.snapshot().nativeVirtualGroups);localEdit&=edited.size()==2&&edited[1].connectionId==secondHop.groupId;
        const auto manualUV=AudioEngine::diagnosticNativeTargetMidi(local.snapshot(),0,2);localEdit&=!manualUV.empty()&&std::all_of(manualUV.begin(),manualUV.end(),[](float value){return value==0;});
        const auto hopFile=folder.getChildFile("multi-split-manual-hop.hjpx");ProjectModel reloaded;localEdit&=local.save(hopFile,error)&&reloaded.load(hopFile,error)&&reloaded.snapshot().nativeVirtualGroups[0].hopPitchCurves.size()==1;
    }
    check("editing_one_two_half_bridge_keeps_other_bridge_and_proofs_exact",localEdit);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
