#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualUnavailableGroupSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");clip.startSeconds=i*.4;clip.durationSeconds=clip.sourceDurationSeconds=.6;clip.sourceTimeMap={{0,0},{.6,.6}};
        {juce::AudioBuffer<float> pcm(1,48000);for(int s=0;s<48000;++s)pcm.setSample(0,s,float(.1*std::sin(s*juce::MathConstants<double>::twoPi*(170+i*33)/48000.)));auto stream=clip.sourceFile.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,48000))return false;}
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=.6;note.nativeUnpitched=true;clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};NativeVirtualGroup good;good.id="valid-route";good.trackId=track.id;good.orderedNoteIds={"note0","note1"};
    NativeVirtualGroup missing;missing.id="missing-member-route";missing.trackId=track.id;missing.orderedNoteIds={"deleted-note","note2"};data.nativeVirtualGroups={good,missing};
    ProjectData projected;std::vector<NativeVirtualRenderGroup> groups;
    check("strict_authoring_validation_still_refuses_missing_members",!prepareNativeVirtualRender(data,projected,groups,error));
    juce::StringArray notices;check("playback_projection_isolates_only_the_unavailable_group",prepareNativeVirtualRender(data,projected,groups,error,&notices)&&groups.size()==1&&groups[0].id==good.id&&notices.size()==1&&notices[0].contains(missing.id));
    auto missingAudio=data;missingAudio.nativeVirtualGroups={good};missingAudio.tracks[0].clips[1].sourceFile=folder.getChildFile("missing-recording.wav");
    juce::StringArray audioNotices;ProjectData audioProjection;std::vector<NativeVirtualRenderGroup> audioGroups;
    check("missing_recording_does_not_steal_a_present_members_contribution",prepareNativeVirtualRender(missingAudio,audioProjection,audioGroups,error,&audioNotices)&&audioGroups.empty()&&audioNotices.size()==1);
    check("unavailable_binding_is_preserved_not_implicitly_dissolved",data.nativeVirtualGroups.size()==2&&projected.nativeVirtualGroups.size()==2&&projected.nativeVirtualGroups[1].orderedNoteIds==missing.orderedNoteIds&&projected.nativeVirtualGroups[1].enabled);
    const auto render=[&](const ProjectData& state,juce::AudioBuffer<float>& buffer,bool expectWarning)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(state);const auto until=juce::Time::getMillisecondCounterHiRes()+10000.;
        while(juce::Time::getMillisecondCounterHiRes()<until){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(2);}
        const auto warning=engine.activeRenderWarnings();const bool diagnosed=expectWarning?warning.contains(missing.id):warning.isEmpty();
        buffer.setSize(2,96000);buffer.clear();engine.setPosition(0);engine.play();for(int offset=0;offset<96000;offset+=257)engine.getNextAudioBlock({&buffer,offset,std::min(257,96000-offset)});engine.stop();
        if(expectWarning){juce::String why;const bool refused=!engine.exportWav(folder.getChildFile("must-not-silently-export.wav"),why);check("export_refuses_unresolved_route_with_explicit_diagnostic",refused&&why.contains(missing.id));}
        return diagnosed;
    };
    juce::AudioBuffer<float> actual,reference;auto validOnly=data;validOnly.nativeVirtualGroups={good};
    const bool rendered=render(data,actual,true)&&render(validOnly,reference,false);bool exact=rendered;
    for(int ch=0;exact&&ch<2;++ch)exact&=std::equal(actual.getReadPointer(ch),actual.getReadPointer(ch)+96000,reference.getReadPointer(ch));
    check("unrelated_audio_and_other_valid_groups_keep_playing_bit_exact",exact&&actual.getMagnitude(0,96000)>.01f);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
