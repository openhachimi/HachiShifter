#pragma once
#include "../NativeVirtualAutoConnect.h"
#include "../SettingsComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualAutoConnectSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto make=[&](juce::String id,double from,double duration,bool bridge=false)
    {
        ClipData clip;clip.id="owner-"+id;clip.sourceFile=output.getChildFile(id+".wav");clip.startSeconds=from;clip.durationSeconds=clip.sourceDurationSeconds=duration;clip.sourceTimeMap={{0,0},{duration,duration}};clip.gain=.7f;clip.fadeInSeconds=.012;clip.fadeOutSeconds=.034;
        NoteData note;note.id=id;note.label="arbitrary alias";note.durationSeconds=duration;note.midiNote=note.sourceMidiCenter=60;note.sourcePitchMeasured=true;
        for(int f=0;f<=int(std::lround(duration/.005));++f)note.contour.push_back({f*.005,0,0,!bridge});
        if(bridge){note.nativeUnpitched=true;note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,duration*.5,"identified-middle"};}
        clip.notes={note};return clip;
    };
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;track.clips={make("left",0,1.2),make("random-bridge",.8,.6,true),make("right",1.,1.)};seed.tracks={track};
    const auto fingerprint=[](const ProjectData& data){ProjectModel model;model.resetDocument(data);return model.contentFingerprint();};
    auto disabled=seed;check("both_optimization_defaults_are_off_and_noop",autoConnectNativeVirtualImport(disabled).created==0&&fingerprint(disabled)==fingerprint(seed));
    disabled=seed;check("pitch_option_alone_does_not_create_connections",autoConnectNativeVirtualImport(disabled,{false,true}).created==0&&fingerprint(disabled)==fingerprint(seed));
    auto bridge=seed;const auto report=autoConnectNativeVirtualImport(bridge,{true,false});
    check("identified_transition_preferred_over_direct_triple_overlap",report.created==1&&bridge.nativeVirtualGroups.size()==1&&bridge.nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","random-bridge","right"}&&bridge.nativeConnections.empty());
    check("automatic_groups_record_provenance_and_no_auto_pitch_by_default",bridge.nativeVirtualGroups[0].automaticallyCreated&&!bridge.nativeVirtualGroups[0].automaticPitch);
    check("existing_group_is_not_rebuilt_or_duplicated",autoConnectNativeVirtualImport(bridge,{true,true}).created==0&&bridge.nativeVirtualGroups.size()==1&&!bridge.nativeVirtualGroups[0].automaticPitch);
    bool originals=true;for(int i=0;i<3;++i){const auto& a=seed.tracks[0].clips[std::size_t(i)];const auto& b=bridge.tracks[0].clips[std::size_t(i)];originals&=a.id==b.id&&a.sourceFile==b.sourceFile&&a.startSeconds==b.startSeconds&&a.sourceDurationSeconds==b.sourceDurationSeconds&&a.gain==b.gain&&a.fadeInSeconds==b.fadeInSeconds&&a.fadeOutSeconds==b.fadeOutSeconds&&a.notes[0].contour.size()==b.notes[0].contour.size();}
    check("automatic_import_uses_same_non_destructive_group_model",originals);
    auto pitch=seed;check("explicit_pitch_option_applies_only_to_new_groups",autoConnectNativeVirtualImport(pitch,{true,true}).created==1&&pitch.nativeVirtualGroups[0].automaticPitch);
    auto direct=seed;direct.tracks[0].clips.erase(direct.tracks[0].clips.begin()+1);
    check("ordinary_cross_source_pair_uses_virtual_authority",autoConnectNativeVirtualImport(direct,{true,false}).created==1&&direct.nativeVirtualGroups.size()==1&&direct.nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"left","right"}&&direct.nativeConnections.empty());
    auto falseName=seed;falseName.tracks[0].clips[1].notes[0].label="e_g";falseName.tracks[0].clips[1].notes[0].pronunciationKind=PronunciationKind::normal;
    check("example_name_is_not_bridge_evidence",autoConnectNativeVirtualImport(falseName,{true,false}).created==0&&falseName.nativeVirtualGroups.empty());
    auto gap=seed;gap.tracks[0].clips.erase(gap.tracks[0].clips.begin()+1);gap.tracks[0].clips[1].startSeconds=3.;check("automatic_new_gap_not_invented",autoConnectNativeVirtualImport(gap,{true,false}).created==0);
    auto simultaneous=seed;simultaneous.tracks[0].clips={make("voice-a",0,1.2),make("voice-b",.1,1.2)};check("long_simultaneous_bodies_not_assumed_melody",autoConnectNativeVirtualImport(simultaneous,{true,false}).created==0);
    auto unknown=seed;unknown.tracks[0].clips[0].notes[0].contour.clear();check("empty_analysis_does_not_fake_endpoint_confidence",autoConnectNativeVirtualImport(unknown,{true,false}).created==0);
    auto falseBreath=seed;falseBreath.tracks[0].clips[1].notes[0].pronunciationKind=PronunciationKind::breath;check("breath_not_promoted_to_transition",autoConnectNativeVirtualImport(falseBreath,{true,false}).created==0);
    auto backing=seed;backing.tracks[0].accompaniment=true;check("backing_track_never_auto_connected",autoConnectNativeVirtualImport(backing,{true,false}).created==0);
    auto offTrack=seed;offTrack.tracks[0].compose=false;check("non_compose_track_never_auto_connected",autoConnectNativeVirtualImport(offTrack,{true,false}).created==0);
    juce::PropertiesFile::Options prefsOptions;prefsOptions.applicationName="virtual-import-options";juce::PropertiesFile props(output.getChildFile("prefs.xml"),prefsOptions);props.clear();I18n strings;juce::AudioDeviceManager devices;SettingsComponent settings(strings,devices,props,[]{});settings.selectPage(4);
    const auto find=[&](auto&& self,juce::Component& c,const juce::String& id)->juce::Component*{if(c.getComponentID()==id)return &c;for(int i=0;i<c.getNumChildComponents();++i)if(auto* result=self(self,*c.getChildComponent(i),id))return result;return nullptr;};
    const auto* connect=dynamic_cast<juce::ToggleButton*>(find(find,settings,"settings-auto-virtual-connections"));const auto* glide=dynamic_cast<juce::ToggleButton*>(find(find,settings,"settings-auto-virtual-pitch"));
    check("real_import_settings_checkboxes_default_off",connect&&glide&&!connect->getToggleState()&&!glide->getToggleState());
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
