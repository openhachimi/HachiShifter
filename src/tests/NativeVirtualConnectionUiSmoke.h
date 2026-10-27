#pragma once
#include "../PianoRollComponent.h"
#include "../NativeLogicalTransitionIO.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualConnectionUiSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String log,error;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int i=0;i<3;++i)
    {const auto file=output.getChildFile(i==0?"left.wav":i==1?"bridge.wav":"right.wav");juce::AudioBuffer<float> pcm(1,144000);for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,.1f*std::sin(juce::MathConstants<double>::twoPi*(173+i*41)*s/48000.));auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,16,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    const std::vector<juce::String> ids{"left","front","back","right"};
    for(int i=0;i<4;++i)
    {
        ClipData clip;clip.id="owner-"+ids[std::size_t(i)];clip.sourceFile=output.getChildFile(i==0?"left.wav":i==3?"right.wav":"bridge.wav");clip.startSeconds=i==0?0.:i==1?.8:i==2?1.1:1.;clip.durationSeconds=clip.sourceDurationSeconds=i==0?1.2:i==3?1.:.3;clip.sourceOffsetSeconds=i==1?1.:i==2?1.3:0.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        NoteData note;note.id=ids[std::size_t(i)];note.label=ids[std::size_t(i)];note.durationSeconds=clip.durationSeconds;note.midiNote=80+float(i);note.nativeUnpitched=true;clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};NativeSplitTransitionEvidence proof;proof.logicalId="one-logical-bridge";proof.frontNoteId="front";proof.backNoteId="back";proof.sourceObjectIdentity=proof.frontSourceObjectIdentity=proof.backSourceObjectIdentity="source-17";
    proof.precedingElementIdentity=proof.frontElementIdentity="element-21";proof.followingElementIdentity=proof.backElementIdentity="element-22";proof.joinsPitches=proof.joinsAmplitudes=proof.joinsPhases=proof.transitionIdentified=true;
    data.logicalTransitions.push_back({proof,1.,1.6,1.3,.9f});ProjectModel model;model.resetDocument(data);const auto group=model.createNativeVirtualConnection(ids,false,error,.1,false,&proof);
    check("real_model_group_created",group.isNotEmpty());if(group.isEmpty())return false;
    I18n strings;strings.setLanguage(I18n::Language::enUS);PianoRollComponent roll(model,strings);roll.setSize(1100,700);roll.setPixelsPerSecond(340);roll.setRowHeight(22);roll.setFocusedTrack(track.id);roll.refreshFromModel();
    const auto paint=[&](const char* name)
    {
        // Fixed software canvas, not the auto-sized whole piano-roll content.
        // Capturing an accelerated backing surface can yield stale grid tiles.
        juce::Image image(juce::Image::ARGB,1100,700,true,juce::SoftwareImageType{});juce::Graphics g(image);
        g.fillAll(Palette::background);roll.paintEntireComponent(g,true);
        auto stream=output.getChildFile(juce::String(name)+".png").createOutputStream();return stream&&juce::PNGImageFormat().writeImageToStream(image,*stream);
    };
    check("physical_members_project_as_one_cached_group_view",roll.diagnosticVirtualGroupViewCount()==1);
    check("logical_halves_have_green_view_identity_without_note_kind_rewrite",roll.diagnosticLogicalTransitionMemberCount()==2&&model.snapshot().tracks[0].clips[1].notes[0].pronunciationKind==PronunciationKind::normal&&!model.snapshot().tracks[0].clips[1].notes[0].nativeTransition);
    check("actual_virtual_connection_preview_written",paint("connected"));
    check("only_two_external_join_markers_no_extra_internal_virtual_edge",roll.diagnosticVirtualMarkerCount()==2);
    const auto menu=roll.diagnosticEnabledNoteMenuIds("front");
    const auto has=[&](int item){return std::find(menu.begin(),menu.end(),item)!=menu.end();};
    check("real_note_menu_exposes_group_operations",has(PianoRollComponent::virtualSelectGroupItem)&&has(PianoRollComponent::virtualToggleGroupItem)&&has(PianoRollComponent::virtualDissolveItem)&&has(PianoRollComponent::virtualExpandItem));
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualSelectGroupItem,0,{});
    const auto selected=roll.selectedNoteIds();check("select_group_action_preserves_all_four_physical_members",selected.size()==4&&std::all_of(ids.begin(),ids.end(),[&](const auto& id){return std::find(selected.begin(),selected.end(),id)!=selected.end();}));
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualExpandItem,0,{});
    check("source_contribution_card_is_view_only",roll.diagnosticExpandedVirtualGroup()==group&&model.snapshot().nativeVirtualGroups.size()==1&&paint("source-contributions"));
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualToggleGroupItem,0,{});
    check("AB_disable_keeps_exact_group_and_source_identities",model.snapshot().nativeVirtualGroups.size()==1&&!model.snapshot().nativeVirtualGroups[0].enabled&&roll.diagnosticVirtualGroupViewCount()==1);
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualToggleGroupItem,0,{});
    check("AB_reenable_uses_same_group",model.snapshot().nativeVirtualGroups[0].enabled&&model.snapshot().nativeVirtualGroups[0].id==group);
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualGeneratePitchItem,0,{});
    check("generate_glide_action_calls_same_group_not_old_graph_writer",model.snapshot().nativeVirtualGroups[0].automaticPitch&&model.snapshot().nativeConnections.empty());
    roll.applyNoteMenuChoice("front",PianoRollComponent::virtualDissolveItem,0,{});
    check("explicit_dissolve_clears_only_virtual_connection",model.snapshot().nativeVirtualGroups.empty()&&model.snapshot().logicalTransitions.size()==1&&roll.diagnosticLogicalTransitionMemberCount()==2&&roll.diagnosticVirtualGroupViewCount()==0&&roll.diagnosticExpandedVirtualGroup().isEmpty());
    model.undo();roll.refreshFromModel();check("undo_dissolve_restores_original_connection_view",model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].id==group&&roll.diagnosticVirtualGroupViewCount()==1);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
