#pragma once
#include "../NativeVirtualConnectionPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualConnectionPlanSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto make=[&](juce::String id,double from,double to,bool transition=false)
    {
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(id+".wav");clip.startSeconds=from;clip.durationSeconds=clip.sourceDurationSeconds=to-from;
        clip.sourceTimeMap={{0,0},{to-from,to-from}};
        NoteData note;note.id=id;note.label="any alias";note.durationSeconds=to-from;note.materialAudioId="audio-"+id;note.materialRegionId="row-"+id;
        if(transition){note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,(to-from)*.5,"test-explicit-middle"};}
        clip.notes={note};return clip;
    };
    TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    track.clips={make("left",0,1.1),make("right",1.,2.)};NativeVirtualConnectionPlan plan;
    check("two_independent_sources_overlap",planNativeVirtualConnection(track,{"left","right"},plan,error)&&plan.handovers.size()==1&&!plan.throughTransition);
    const auto midpoint=nativeVirtualContributions(plan,1.05);
    check("one_direct_partition_not_two_full_signals",std::abs(midpoint[0]-.5)<1.e-8&&std::abs(midpoint[1]-.5)<1.e-8&&midpoint[2]==0);
    track.clips={make("left",0,1.2),make("arbitrary-transition-name",.8,1.4,true),make("right",1.,2.)};
    const auto ids=std::vector<juce::String>{"left","arbitrary-transition-name","right"};
    check("identified_transition_routes_triple_overlap_without_alias_matching",planNativeVirtualConnection(track,ids,plan,error)&&plan.throughTransition&&plan.handovers.size()==2&&plan.handovers[0].to<=1.1+1.e-8&&plan.handovers[1].from>=1.1-1.e-8);
    bool partition=true;for(double t=0;t<2.;t+=.001){const auto w=nativeVirtualContributions(plan,t);partition&=std::abs(w[0]+w[1]+w[2]-1.)<1.e-8;for(const auto x:w)partition&=x>=0&&x<=1;}
    check("triple_overlap_is_one_partition_never_adds_direct_route_twice",partition);
    const auto outside=nativeVirtualContributions(plan,-.01);check("no_audio_invented_outside_source_support",outside[0]==0&&outside[1]==0&&outside[2]==0);
    auto untyped=track;untyped.clips[1].notes[0].pronunciationKind=PronunciationKind::normal;untyped.clips[1].notes[0].label="e_g";
    // User-approved arbitrary-member MANUAL chains are now valid; a name
    // still cannot turn their middle ordinary note into a classified bridge.
    check("example_alias_does_not_classify_an_ordinary_manual_chain_as_transition",
        planNativeVirtualConnection(untyped,ids,plan,error)&&!plan.throughTransition&&!plan.members[1].transition);
    auto markerless=track;markerless.clips[1].notes[0].nativeTransition.reset();
    check("missing_middle_is_not_guessed",!planNativeVirtualConnection(markerless,ids,plan,error));
    auto poly=track;poly.clips.push_back(make("harmony",.85,1.15));
    check("fourth_voice_is_not_silently_absorbed",!planNativeVirtualConnection(poly,ids,plan,error));
    check("duplicate_ids_refused",!planNativeVirtualConnection(track,{"left","left"},plan,error));
    check("reversed_order_refused",!planNativeVirtualConnection(track,{"right","arbitrary-transition-name","left"},plan,error));
    auto touching=track;touching.clips={make("left",0,1.),make("right",1.,2.)};
    check("touching_sources_need_no_fake_overlap",planNativeVirtualConnection(touching,{"left","right"},plan,error)&&plan.handovers[0].from==plan.handovers[0].to);
    const auto cut=nativeVirtualContributions(plan,1.);check("touching_cut_routes_once",cut[0]==0&&cut[1]==1);
    auto gap=touching;gap.clips[1].startSeconds=1.01;
    check("automatic_gap_fill_forbidden",!planNativeVirtualConnection(gap,{"left","right"},plan,error));
    auto same=touching;same.clips[1].sourceFile=same.clips[0].sourceFile;same.clips[1].sourceOffsetSeconds=1.;
    check("genuinely_contiguous_source_slices_keep_partial_cut_authority",!planNativeVirtualConnection(same,{"left","right"},plan,error));
    same.clips[1].sourceOffsetSeconds=2.;
    check("same_recording_skipping_a_middle_source_region_allows_virtual_connection",planNativeVirtualConnection(same,{"left","right"},plan,error)&&plan.members[0].sourceToSeconds==1.&&plan.members[1].sourceFromSeconds==2.);
    auto shifted=track;for(auto& c:shifted.clips)c.startSeconds+=3.;
    check("moving_group_reprojects_all_handover_times",planNativeVirtualConnection(shifted,ids,plan,error)&&std::abs(plan.members[1].middle-4.1)<1.e-8);
    // No modifications to input music, gains, kind or clocks are made by plans.
    check("source_input_remains_unchanged",track.clips[0].startSeconds==0&&std::abs(track.clips[1].notes[0].nativeTransition->boundarySourceSeconds-.3)<1.e-8&&track.clips[2].gain==1.f);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
