#pragma once
#include "../ProjectModel.h"
#include "../NativeOverlapConnection.h"
#include "../AudioEngine.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
// Model + normal PCM mixer regression. Sines are deterministic source controls,
// not an ASR/voice-quality oracle; this test never invokes a different renderer.
inline bool runNativeForceConnectionSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("native-force-connection-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log+=name+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto eq=[](double a,double b){return std::abs(a-b)<1.e-7;};
    const auto smooth=[](double x){x=juce::jlimit(0.0,1.0,x);return float(x*x*(3-2*x));};
    juce::File sources[2]={root.getChildFile("a.wav"),root.getChildFile("b.wav")};
    for(int j=0;j<2;++j)
    {
        juce::AudioBuffer<float> pcm(1,67200);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,.12f*std::sin(juce::MathConstants<double>::twoPi*(j?311:173)*i/48000.0));
        auto stream=sources[j].createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,24,{},0):nullptr);
        check("physical_source_"+juce::String(j),writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));
    }
    const auto hashA=juce::SHA256(sources[0]).toHexString(),hashB=juce::SHA256(sources[1]).toHexString();
    const auto fixture=[&](bool unpitched,bool profiles)
    {
        ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;track.nativeNsfAudio=true;
        track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.normalizeVolume=false;track.smoothOverlaps=false;track.allowNativeAudioOverlap=true;
        track.renderOrder=RenderOrder::processThenSplice;track.mergeContiguousSource=false;
        for(int i=0;i<2;++i)
        {
            ClipData clip;clip.id="clip-"+juce::String(i);clip.sourceFile=sources[i];clip.startSeconds=i*.75;clip.durationSeconds=clip.sourceDurationSeconds=1;
            clip.sourceOffsetSeconds=.1;clip.sourceTimeMap={{0,0},{1,1}};clip.fadeInSeconds=i?.13:.17;clip.fadeOutSeconds=i?.11:.07;
            NoteData note;note.id=i?"B":"A";note.label=i?"b":"a";note.durationSeconds=1;note.nativeUnpitched=unpitched;
            note.midiNote=note.sourceMidiCenter=i?67.f:60.f;note.sourcePitchMeasured=true;note.contour={{0,0,0,!unpitched},{1,0,0,!unpitched}};
            note.pitchControlPoints={{0,note.midiNote,PitchCurveShape::linear},{1,note.midiNote,PitchCurveShape::linear}};
            note.materialAudioId=i?"variant-b":"variant-a";note.materialRegionId=i?"row-b":"row-a";
            note.materialSourceStartSeconds=note.nativeSourceStartSeconds=.1;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=1.1;
            note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.1;note.materialBoundAlias=note.label;note.materialAliasFollowsPrimary=note.materialLabelFollowsAlias=true;
            if(profiles){NativePronunciationEdges edge;edge.entryFromSourceSeconds=.1;edge.entryToSourceSeconds=.2;edge.exitFromSourceSeconds=.9;edge.exitToSourceSeconds=edge.terminationSourceSeconds=1.1;edge.provenance="explicit-source-anchors";note.nativePronunciationEdges=edge;}
            clip.notes={note};track.clips.push_back(std::move(clip));
        }
        data.tracks={track};return data;
    };
    ProjectModel model;const auto initial=fixture(false,true);model.resetDocument(initial);const auto before=model.contentFingerprint();const auto revision=model.revisionNumber();
    check("old_pitch_only_API_still_rejects_overlap_atomically",!model.setNotesPortamento({"A","B"},true,error)&&error.containsIgnoreCase("overlap")&&model.contentFingerprint()==before&&!model.canUndo());
    check("force_actual_native_source_compound",model.forceNativeConnection({"A","B"},true,error));
    const auto joined=model.snapshot();
    bool geometry=joined.tracks[0].clips.size()==1&&joined.nativeConnections.size()==1;
    if(geometry)
    {
        const auto& parent=joined.tracks[0].clips[0];geometry=parent.nativeAudioLinked&&parent.parts.size()==2&&parent.notes.size()==2&&eq(parent.durationSeconds,1.75);
        const auto expanded=expandedClipParts(parent);
        for(std::size_t i=0;geometry&&i<expanded.size();++i)
        {
            const auto& old=initial.tracks[0].clips[i];const auto& now=expanded[i];
            geometry&=now.sourceFile==old.sourceFile&&eq(now.startSeconds,old.startSeconds)&&eq(now.durationSeconds,old.durationSeconds)
                &&eq(now.sourceOffsetSeconds,old.sourceOffsetSeconds)&&eq(now.sourceDurationSeconds,old.sourceDurationSeconds)
                &&eq(now.fadeInSeconds,old.fadeInSeconds)&&eq(now.fadeOutSeconds,old.fadeOutSeconds)
                &&now.notes[0].id==old.notes[0].id&&eq(now.notes[0].startSeconds,old.notes[0].startSeconds)&&eq(now.notes[0].durationSeconds,old.notes[0].durationSeconds)
                &&now.notes[0].midiNote==old.notes[0].midiNote&&now.notes[0].pitchControlPoints.size()==old.notes[0].pitchControlPoints.size()
                &&now.notes[0].nativePronunciationEdges==old.notes[0].nativePronunciationEdges
                &&eq(now.notes[0].materialSourceStartSeconds,.1)&&eq(now.notes[0].materialSourceEndSeconds,1.1);
            for(std::size_t k=0;geometry&&k<now.sourceTimeMap.size();++k)geometry&=eq(now.sourceTimeMap[k].targetSeconds,old.sourceTimeMap[k].targetSeconds)&&eq(now.sourceTimeMap[k].sourceSeconds,old.sourceTimeMap[k].sourceSeconds);
        }
        check("no_score_body_source_support_controls_or_author_fades_trimmed",geometry);
        check("actual_overlap_separate_crossfade_multipliers",eq(parent.parts[0].crossfadeOutSeconds,.25)&&eq(parent.parts[1].crossfadeInSeconds,.25));
        ResolvedNativeOverlapBoundary proof;check("one_shared_actual_boundary_proof",resolveNativeOverlapBoundary(joined.tracks[0],joined.nativeConnections[0],proof,error)
            &&eq(proof.overlapFromTargetSeconds,.75)&&eq(proof.overlapToTargetSeconds,1)&&eq(proof.boundaryTargetSeconds,.875)
            &&proof.parentClipId==parent.id&&proof.leftOwnerId==parent.notes[0].clipPartId&&proof.rightOwnerId==parent.notes[1].clipPartId);
        const auto curves=resolvedConnectionPitchCurves(joined.tracks[0],joined.nativeConnections);
        check("typed_bridge_computed_inside_real_overlap",curves.size()==1&&curves[0].active&&eq(curves[0].fromSeconds,.825)&&eq(curves[0].toSeconds,.925)
            &&eq(curves[0].points.front().targetMidi,60)&&eq(curves[0].points.back().targetMidi,67)&&joined.nativeConnections[0].type=="native-overlap");
        if(!curves.empty()&&curves[0].active)check("typed_bridge_remains_editable_via_existing_graph_authority",!model.setConnectionPitchCurve(joined.nativeConnections[0].id,curves[0].points,error)&&error.isEmpty());
    }
    else check("one_compound_and_one_edge",false);
    check("one_atomic_music_undo",model.revisionNumber()==revision+1&&model.undo()&&!model.canUndo()&&model.contentFingerprint()==before);
    check("redo_actual_compound",model.redo());
    const auto afterRedo=model.contentFingerprint();const auto afterRevision=model.revisionNumber();
    check("repeat_is_noop",!model.forceNativeConnection({"A","B"},true,error)&&error.isEmpty()&&model.contentFingerprint()==afterRedo&&model.revisionNumber()==afterRevision);
    const auto file=root.getChildFile("forced-overlap.hjpx");ProjectModel cold;
    check("HJPX_cold_reopen",model.save(file,error)&&cold.load(file,error));
    const auto coldData=cold.snapshot();ResolvedNativeOverlapBoundary coldProof;
    check("HJPX_keeps_typed_exact_boundary_and_live_pitch",coldData.nativeConnections.size()==1&&resolveNativeOverlapBoundary(coldData.tracks[0],coldData.nativeConnections[0],coldProof,error)
        &&eq(coldProof.boundaryTargetSeconds,.875)&&resolvedConnectionPitchCurves(coldData.tracks[0],coldData.nativeConnections)[0].active);
    ProjectModel moving;moving.resetDocument(joined);
    const bool moved=moving.moveNativeNotes({"A","B"},.4,0,&error);const auto movedData=moving.snapshot();ResolvedNativeOverlapBoundary movedProof;
    const auto movedCurves=resolvedConnectionPitchCurves(movedData.tracks[0],movedData.nativeConnections);
    check("whole_compound_native_note_move_recomputes_live_cutline_and_bridge",moved&&movedData.nativeConnections.size()==1
        &&resolveNativeOverlapBoundary(movedData.tracks[0],movedData.nativeConnections[0],movedProof,error)
        &&eq(movedProof.boundaryTargetSeconds,1.275)&&eq(movedProof.overlapFromTargetSeconds,1.15)&&eq(movedProof.overlapToTargetSeconds,1.4)
        &&movedCurves.size()==1&&movedCurves[0].active&&eq(movedCurves[0].fromSeconds,1.225)&&eq(movedCurves[0].toSeconds,1.325));
    ProjectModel movedCold;const auto movedFile=root.getChildFile("forced-overlap-moved.hjpx");
    const bool movedSaved=moving.save(movedFile,error)&&movedCold.load(movedFile,error);const auto movedColdData=movedCold.snapshot();ResolvedNativeOverlapBoundary movedColdProof;
    check("moved_cutline_is_live_after_HJPX_reopen_not_saved_pixel",movedSaved&&movedColdData.nativeConnections.size()==1
        &&resolveNativeOverlapBoundary(movedColdData.tracks[0],movedColdData.nativeConnections[0],movedColdProof,error)&&eq(movedColdProof.boundaryTargetSeconds,1.275));
    check("disable_pitch_retains_nondestructive_physical_assembly",cold.forceNativeConnection({"A","B"},false,error)
        &&cold.snapshot().tracks[0].clips[0].nativeAudioLinked&&cold.snapshot().nativeConnections[0].type=="native-overlap"&&cold.snapshot().nativeConnections[0].pitchCurve.empty());
    const auto negative=[&](const juce::String& name,ProjectData data,const std::vector<juce::String>& ids)
    {ProjectModel bad;bad.resetDocument(data);const auto fingerprint=bad.contentFingerprint();const auto rev=bad.revisionNumber();check(name,!bad.forceNativeConnection(ids,true,error)&&error.isNotEmpty()&&bad.contentFingerprint()==fingerprint&&bad.revisionNumber()==rev&&!bad.canUndo());};
    negative("duplicate_IDs_atomic",initial,{"A","A"});negative("missing_ID_atomic",initial,{"A","missing"});
    auto cross=initial;auto extra=cross.tracks[0];extra.id="other";extra.clips={cross.tracks[0].clips[1]};cross.tracks[0].clips.resize(1);cross.tracks.push_back(extra);negative("cross_track_atomic",cross,{"A","B"});
    auto nested=initial;nested.tracks[0].clips[1].durationSeconds=.2;nested.tracks[0].clips[1].notes[0].durationSeconds=.2;negative("nested_overlap_atomic",nested,{"A","B"});
    auto rest=initial;rest.tracks[0].clips[1].notes[0].label="RR";negative("rest_voicing_atomic",rest,{"A","B"});
    auto third=initial;auto interference=third.tracks[0].clips[0];interference.id="third";interference.startSeconds=.8;interference.notes[0].id="C";third.tracks[0].clips.push_back(interference);negative("third_voice_atomic",third,{"A","B"});
    auto spoof=initial;NativeConnection fake;fake.id="forged";fake.type="native-overlap";fake.leftNoteId="A";fake.rightNoteId="B";fake.boundarySeconds=.875;fake.pitchCurveEndpointRelative=true;fake.pitchCurve={{0,0},{1,0}};
    const auto rejected=resolvedConnectionPitchCurves(spoof.tracks[0],{fake});check("type_string_alone_never_bypasses_compound_proof",rejected.size()==1&&!rejected[0].active);
    const auto render=[&](const ProjectData& data,juce::AudioBuffer<float>& pcm)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);const auto until=juce::Time::getMillisecondCounterHiRes()+30000;
        for(;;){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;if(juce::Time::getMillisecondCounterHiRes()>until)return false;juce::Thread::sleep(2);}
        if(engine.activeRenderWarnings().isNotEmpty())return false;pcm.setSize(2,84000);pcm.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<84000;offset+=257)engine.getNextAudioBlock({&pcm,offset,std::min(257,84000-offset)});
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    for(const bool automatic:{false,true})for(const bool profiles:{false,true})
    {
        auto raw=fixture(true,profiles);raw.tracks[0].smoothOverlaps=automatic;ProjectModel assembly;assembly.resetDocument(raw);
        const auto name=juce::String(automatic?"smooth_enabled_":"smooth_disabled_")+juce::String(profiles?"profiled_":"legacy_NONE_");
        const bool forced=assembly.forceNativeConnection({"A","B"},true,error);check(name+"unpitched_assembly_never_invents_voicing",forced&&assembly.snapshot().nativeConnections.size()==1&&assembly.snapshot().nativeConnections[0].pitchCurve.empty());
        auto controlA=raw,controlB=raw;controlA.tracks[0].clips.resize(1);controlB.tracks[0].clips.erase(controlB.tracks[0].clips.begin());
        for(auto* data:{&controlA,&controlB})for(auto& clip:data->tracks[0].clips)for(auto& note:clip.notes)note.nativePronunciationEdges.reset();
        juce::AudioBuffer<float> a,b,actual;bool exact=forced&&render(controlA,a)&&render(controlB,b)&&render(assembly.snapshot(),actual);float peak=0;
        for(int i=0;exact&&i<84000;++i)
        {
            const auto t=i/48000.0;float ga=float(juce::jlimit(0.0,1.0,(1-t)/.25)),gb=float(juce::jlimit(0.0,1.0,(t-.75)/.25));
            if(profiles){ga*=smooth(t/.1);gb*=smooth((1.75-t)/.2);}
            for(int channel=0;channel<2;++channel)
            {const auto expected=a.getSample(channel,i)*ga+b.getSample(channel,i)*gb;peak=std::max(peak,std::abs(expected));exact&=std::abs(actual.getSample(channel,i)-expected)<3.e-6f;}
        }
        check(name+"actual_normal_PCM_two_sources_complementary_overlap_author_fades_multiply",exact&&peak>.01f);
    }
    check("physical_source_PCM_unchanged",juce::SHA256(sources[0]).toHexString()==hashA&&juce::SHA256(sources[1]).toHexString()==hashB);
    const bool written=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_force_connection_fixture="<<root.getFullPathName()<<std::endl;return ok&&written;
}
}
