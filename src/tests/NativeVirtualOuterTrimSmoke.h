#pragma once
#include "../NativeCoupledSelectionMove.h"
#include "../NativeVirtualRenderPlan.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
// Intentionally NOT the permanent-seam resize smoke: every GUI gesture uses
// Tool::trim and asserts nativeTrimDrag before moving the pointer.
inline bool runNativeVirtualOuterTrimSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;
    bool ok=true;juce::String error,log;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" | "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto wave=folder.getChildFile("trim-source.wav");
    {juce::AudioBuffer<float> pcm(1,240000);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,float(.08*std::sin(i*juce::MathConstants<double>::twoPi*220/48000.)));auto stream=wave.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    ProjectData plain;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner-"+juce::String(i);clip.sourceFile=folder.getChildFile("source-"+juce::String(i)+".wav");if(!wave.copyFileTo(clip.sourceFile))return false;
        clip.startSeconds=1+i;clip.durationSeconds=clip.sourceDurationSeconds=1.1;clip.sourceTimeMap={{0,0},{.4,.25},{1.1,1.1}};
        NoteData note;note.id=juce::String::charToString(juce::juce_wchar('a'+i));note.durationSeconds=1.1;note.midiNote=note.sourceMidiCenter=60+float(i*3);note.sourcePitchMeasured=true;
        for(int f=0;f<=220;++f)note.contour.push_back({f*.005,0,0,true});
        note.pitchControlPoints={{0,note.midiNote,PitchCurveShape::natural},{.5,note.midiNote+.2f,PitchCurveShape::natural},{1.1,note.midiNote,PitchCurveShape::natural}};
        note.amplitudeEnvelope={{0,-4},{.4,-1},{1.1,-3}};
        NativeSegment segment;segment.id="author-segment";segment.sourceStartSeconds=0;segment.sourceEndSeconds=1.1;segment.provenance="user";note.nativeSegments={segment};
        clip.notes={note};track.clips.push_back(clip);
    }
    plain.tracks={track};ProjectModel model;model.resetDocument(plain);
    check("create_a_b_virtual_route",model.setNativeVirtualConnectionForNotes({"a","b"},true,false,error));
    if(model.snapshot().nativeVirtualGroups.empty())return false;
    const auto connected=model.snapshot();
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setTool(PianoRollComponent::Tool::trim);roll.setShowNativeWaveforms(false);roll.setPixelsPerSecond(400);roll.setRowHeight(26);roll.setSize(2400,3600);
    const auto paint=[&]{juce::Image image(juce::Image::ARGB,2300,400,true,juce::SoftwareImageType{});juce::Graphics g(image);g.addTransform(juce::AffineTransform::translation(0,-float(roll.diagnosticYForMidi(69))+20));roll.paintEntireComponent(g,true);};
    const auto event=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
    const auto mouseTrim=[&](const ProjectData& seed,const char* id,bool left,double delta,bool allowed,const juce::String& tag)
    {
        model.resetDocument(seed);model.dispatchPendingMessages();roll.diagnosticRefresh();paint();
        std::optional<juce::Rectangle<float>> bounds;for(std::size_t i=0;i<roll.diagnosticHitCount();++i)if(roll.diagnosticHitNoteId(i)==id)bounds=roll.diagnosticHitNominalBounds(i);
        const auto before=model.contentFingerprint();juce::String rejected;roll.onEditRejected=[&](const juce::String& text){rejected=text;};
        if(!bounds){check(tag+"_hit_exists",false);roll.onEditRejected=nullptr;return;}
        const juce::Point<float> down(left?bounds->getX()+2:bounds->getRight()-2,bounds->getCentreY());const auto up=down.translated(float(delta*400),0);
        roll.mouseDown(event(down,down));check(tag+"_uses_actual_trim_tool",roll.diagnosticNativeTrimDragging()==allowed);
        roll.mouseDrag(event(up,down));paint();check(tag+"_preview_has_no_model_commit",before==model.contentFingerprint());
        roll.mouseUp(event(up,down));model.dispatchPendingMessages();roll.diagnosticRefresh();
        check(tag+"_commit",(before!=model.contentFingerprint())==allowed);
        if(!allowed)check(tag+"_disconnect_hint",rejected==I18n::uiText("seam.disconnectBeforeCrop"));
        else
        {
            auto expected=seed;bool planned=false;for(auto& t:expected.tracks)for(auto& c:t.clips)if(auto plan=planNativeNoteTrim(c,id,delta,left,t.allowNativeAudioOverlap)){c=std::move(plan->clip);planned=true;}
            ProjectModel oracle;oracle.resetDocument(expected);check(tag+"_author_segments_and_clock_equal_existing_crop_plan",planned&&oracle.contentFingerprint()==model.contentFingerprint());
            const auto applied=model.contentFingerprint();model.undo();check(tag+"_undo",before==model.contentFingerprint());model.redo();check(tag+"_redo",applied==model.contentFingerprint());
            const auto saved=folder.getChildFile(tag+".hjpx");ProjectModel cold;const bool loaded=model.save(saved,error)&&cold.load(saved,error);check(tag+"_cold_load",loaded&&cold.contentFingerprint()==applied);
            if(loaded){const auto coldBefore=cold.contentFingerprint();check(tag+"_cold_edit",cold.trimNativeNoteEdge(id,left?.03:-.03,left));const auto coldAfter=cold.contentFingerprint();cold.undo();check(tag+"_cold_undo",cold.contentFingerprint()==coldBefore);cold.redo();check(tag+"_cold_redo",cold.contentFingerprint()==coldAfter);}
            const auto data=model.snapshot();bool valid=true;for(const auto& t:data.tracks)for(const auto& c:t.clips)for(const auto& source:expandedClipParts(c)){const auto clock=nativeClipClock(source);for(std::size_t i=1;i<clock.size();++i)valid&=clock[i].targetSeconds>clock[i-1].targetSeconds&&clock[i].sourceSeconds>clock[i-1].sourceSeconds;}
            for(const auto& group:data.nativeVirtualGroups){NativeVirtualConnectionPlan p;valid&=resolveNativeVirtualGroup(data.tracks[0],group,p,error);}
            check(tag+"_positive_source_clock_and_route_resolve",valid);
            if(!data.nativeVirtualGroups.empty()&&data.nativeVirtualGroups[0].enabled)
            {const auto lines=overlayNativeVirtualSingleLine(data.tracks[0],sharedPitchLines(data.tracks[0],data.nativeConnections),data.nativeVirtualGroups);const auto* a=lines.memberFor("a");const auto* b=lines.memberFor("b");check(tag+"_one_shared_F0",a&&b&&a->line&&a->line==b->line);}
        }
        roll.onEditRejected=nullptr;
    };
    for(int enabled:{1,0})
    {
        auto seed=connected;seed.nativeVirtualGroups[0].enabled=enabled!=0;seed.nativeVirtualGroups[0].coupledMovement=false;
        const auto tag=juce::String(enabled?"enabled":"disabled");
        mouseTrim(seed,"a",true,.2,true,tag+"_a_left_outer");mouseTrim(seed,"b",false,-.2,true,tag+"_b_right_outer");
        mouseTrim(seed,"a",false,-.2,false,tag+"_a_right_join");mouseTrim(seed,"b",true,.2,false,tag+"_b_left_join");
    }
    // A virtual last member can ALSO have a real outgoing edge. Neither a
    // missing compatibility flag nor being last in the group permits cropping.
    auto successor=connected;NativeConnection real;real.id="real-b-c";real.leftNoteId="b";real.rightNoteId="c";successor.nativeConnections.push_back(real);
    mouseTrim(successor,"b",false,-.2,false,"graph_real_successor");
    auto flags=connected;flags.tracks[0].clips[1].notes[0].connectedToNext=true;mouseTrim(flags,"b",false,-.2,false,"flag_real_successor");
    auto glide=connected;glide.tracks[0].clips[1].glideConnectedToNext=true;mouseTrim(glide,"b",false,-.2,false,"glide_real_successor");
    // An incoming graph at b is NOT its right edge; preserve it exactly.
    auto incoming=connected;real.id="keep-a-b";real.leftNoteId="a";real.rightNoteId="b";incoming.nativeConnections={real};
    mouseTrim(incoming,"b",false,-.2,true,"opposite_graph_preserved");
    mouseTrim(plain,"a",true,.2,true,"unlinked_a_left");mouseTrim(plain,"a",false,-.2,true,"unlinked_a_right");
    mouseTrim(plain,"b",true,.2,true,"unlinked_b_left");mouseTrim(plain,"b",false,-.2,true,"unlinked_b_right");
    // Real multi-note owner: no blanket nativeAudioLinked/member rejection.
    auto multi=plain;auto owner=multi.tracks[0].clips[0];owner.durationSeconds=owner.sourceDurationSeconds=2.2;owner.sourceTimeMap={{0,0},{2.2,2.2}};
    auto b=multi.tracks[0].clips[1].notes[0];b.startSeconds=1.1;owner.notes[0].connectedToNext=true;b.connectedToPrevious=true;owner.notes.push_back(b);multi.tracks[0].clips={owner};
    mouseTrim(multi,"a",true,.2,true,"same_owner_a_outer");mouseTrim(multi,"b",false,-.2,true,"same_owner_b_outer");mouseTrim(multi,"b",true,.2,false,"same_owner_b_join");
    // Source-part glide cannot disappear behind an unflagged parent note.
    auto nested=plain;auto parent=assembledLinkedAudio({plain.tracks[0].clips[0],plain.tracks[0].clips[1]});parent.nativeAudioLinked=false;
    for(auto& part:parent.parts)part.glideConnectedToNext=true;nested.tracks[0].clips={parent};model.resetDocument(nested);const auto fp=model.contentFingerprint();
    check("part_glide_blocks_outer_crop",!model.trimNativeNoteEdge("b",-.2,false)&&fp==model.contentFingerprint());
    // A complete physical two-half bridge goes through the actual trim tool,
    // not just synthetic edge predicates. Its normal outer edges remain free.
    auto split=plain;
    for(int half=0;half<2;++half)
    {
        auto clip=plain.tracks[0].clips[2];clip.id=half?"back-owner":"front-owner";clip.sourceFile=wave;clip.startSeconds=1.95+half*.1;clip.durationSeconds=clip.sourceDurationSeconds=.1;clip.sourceOffsetSeconds=half*.1;clip.sourceTimeMap={{0,0},{.1,.1}};
        auto note=clip.notes[0];note.id=half?"back":"front";note.durationSeconds=.1;note.nativeSegments.clear();note.pitchControlPoints.clear();note.amplitudeEnvelope.clear();note.contour.clear();note.nativeUnpitched=half!=0;
        for(int f=0;f<=20;++f)note.contour.push_back({f*.005,0,0,half==0});clip.notes={note};split.tracks[0].clips.push_back(clip);
    }
    NativeSplitTransitionEvidence splitProof;splitProof.logicalId="typed-two-halves";splitProof.frontNoteId="front";splitProof.backNoteId="back";splitProof.sourceObjectIdentity=splitProof.frontSourceObjectIdentity=splitProof.backSourceObjectIdentity="physical-bridge";
    splitProof.precedingElementIdentity=splitProof.frontElementIdentity="front-element";splitProof.followingElementIdentity=splitProof.backElementIdentity="back-element";splitProof.joinsPitches=splitProof.joinsAmplitudes=splitProof.joinsPhases=splitProof.transitionIdentified=true;
    model.resetDocument(split);const bool splitBound=model.setNativeVirtualConnectionForNotes({"a","front","back","b"},true,false,error,.1,false,&splitProof);check("bind_real_typed_halves",splitBound);
    if(splitBound)
    {
        split=model.snapshot();mouseTrim(split,"a",true,.15,true,"typed_a_outer");mouseTrim(split,"b",false,-.15,true,"typed_b_outer");
        mouseTrim(split,"front",true,.02,false,"typed_front_left");mouseTrim(split,"front",false,-.02,false,"typed_front_right");mouseTrim(split,"back",true,.02,false,"typed_back_left");mouseTrim(split,"back",false,-.02,false,"typed_back_right");
    }
    // Directed typed two-half bridge edges, including the retained extra-proof
    // path, are guarded independently of source pitch/voicing or enabled state.
    auto typed=connected;auto& group=typed.nativeVirtualGroups[0];group.orderedNoteIds={"a","front","back","b"};group.enabled=false;
    NativeSplitTransitionEvidence proof;proof.frontNoteId="front";proof.backNoteId="back";group.splitTransition=proof;
    const auto& t=typed.tracks[0];
    check("typed_front_both_join_edges",nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"front",true)&&nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"front",false));
    check("typed_back_both_join_edges",nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"back",true)&&nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"back",false));
    check("typed_outer_normal_edges_remain_free",!nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"a",true)&&!nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"b",false));
    group.splitTransition.reset();group.extraSplitTransitions={proof};group.orderedNoteIds={"a","b"};
    check("retained_extra_proof_direction",nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"front",false)&&nativeNoteEdgeHasConnectedSeam(t,typed.nativeVirtualGroups,{},"back",true));
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
