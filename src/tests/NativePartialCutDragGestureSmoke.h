#pragma once
#include "../PianoRollComponent.h"
#include "../NativeNoteTiming.h"
#include <iostream>
namespace hachi
{
// The ACTUAL PianoRoll drag gesture, not a direct model call: a pure vertical
// drag takes a different code path (transposeNotes) than a horizontal one, and
// both must keep an imported partial cut joined.
inline bool runNativePartialCutDragGestureSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto source=output.getChildFile("phrase.wav");
    {
        juce::AudioBuffer<float> pcm(1,96000);
        for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,.1f*std::sin(juce::MathConstants<double>::twoPi*196*s/48000.));
        auto stream=source.createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));
        if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;
    }
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;
    track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.compose=true;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id=i?"right-owner":"left-owner";clip.sourceFile=source;
        clip.startSeconds=i?1.4:1.;clip.durationSeconds=clip.sourceDurationSeconds=i?.6:.4;
        clip.sourceOffsetSeconds=i?.4:0.;clip.sourceTimeMap={{0,0},{clip.durationSeconds,clip.durationSeconds}};
        clip.materialSourceUsageGroupId="melodyne-use-1";
        clip.glideConnectedToNext=i==0;clip.glideConnectedFromPrevious=i==1;
        NoteData note;note.id=i?"right":"left";note.label="-";note.durationSeconds=clip.durationSeconds;
        note.midiNote=note.sourceMidiCenter=i?67:62;note.sourcePitchMeasured=true;
        note.connectedToNext=i==0;note.connectedToPrevious=i==1;
        for(int f=0;f<=int(std::lround(note.durationSeconds/.005));++f)note.contour.push_back({f*.005,0,0,true});
        clip.notes={note};track.clips.push_back(clip);
    }
    data.tracks={track};ProjectModel model;model.resetDocument(data);
    I18n strings;strings.setLanguage(I18n::Language::enUS);
    PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");
    roll.setTool(PianoRollComponent::Tool::note);roll.setPixelsPerSecond(400);roll.setRowHeight(26);
    roll.setSize(1400,900);roll.diagnosticRefresh();
    const auto seam=[&](double& targetGap,double& sourceGap)
    {
        const auto state=model.snapshot();const ClipData *left=nullptr,*right=nullptr;
        for(const auto& clip:state.tracks[0].clips)for(const auto& note:clip.notes)
        {if(note.id=="left")left=&clip;if(note.id=="right")right=&clip;}
        if(!left||!right){error="Seam endpoints are no longer present in the project";return false;}
        const auto lc=nativeClipClock(*left),rc=nativeClipClock(*right);
        const auto& a=left->notes.front();const auto& b=right->notes.front();
        targetGap=(right->startSeconds+b.startSeconds)-(left->startSeconds+a.startSeconds+a.durationSeconds);
        sourceGap=(right->sourceOffsetSeconds+nativeSourceTimeAt(rc,b.startSeconds))
            -(left->sourceOffsetSeconds+nativeSourceTimeAt(lc,a.startSeconds+a.durationSeconds));
        return true;
    };
    const auto midiOf=[&](const juce::String& id)
    {
        for(const auto& track:model.snapshot().tracks)for(const auto& clip:track.clips)
            for(const auto& note:clip.notes)if(note.id==id)return note.midiNote;
        return -1.0f;
    };
    {
        const auto state=model.snapshot();
        std::cout<<"fixture_tracks="<<state.tracks.size()
            <<" clips="<<(state.tracks.empty()?0:int(state.tracks[0].clips.size()))
            <<" hits="<<roll.diagnosticHitCount()<<std::endl;
    }
    const auto event=[&](juce::Point<float> at,juce::Point<float> down)
    {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,
            &roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);
    };
    const auto hitOf=[&](const juce::String& id)->std::optional<juce::Point<float>>
    {
        for(std::size_t i=0;i<roll.diagnosticHitCount();++i)
        {
            if(roll.diagnosticHitNoteId(i)==id)return roll.diagnosticHitNominalBounds(i).getCentre();
        }
        return std::nullopt;
    };
    const auto gesture=[&](const juce::String& id,double deltaSeconds,int rows,const char* name)
    {
        roll.diagnosticRefresh();
        const auto at=hitOf(id);
        if(!at){error="Could not locate the note in the real roll";check(name,false);return;}
        roll.mouseDown(event(*at,*at));
        const auto to=at->translated(float(deltaSeconds*400.),float(-rows*26));
        // Intermediate samples: a single jump can be read as a click.
        roll.mouseDrag(event(at->translated((to.x-at->x)*.5f,(to.y-at->y)*.5f),*at));
        roll.mouseDrag(event(to,*at));roll.mouseUp(event(to,*at));
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        std::cout<<name<<" from=("<<at->x<<","<<at->y<<") to=("<<to.x<<","<<to.y<<")"
            <<" rightMidi="<<midiOf("right")<<" leftMidi="<<midiOf("left")<<std::endl;
        double targetGap=1,sourceGap=1;const auto measured=seam(targetGap,sourceGap);
        std::cout<<name<<" target_gap="<<targetGap<<" source_gap="<<sourceGap<<std::endl;
        check(name,measured&&std::abs(targetGap)<1.e-6&&std::abs(sourceGap)<1.e-6);
    };
    double startTarget=1,startSource=1;
    check("gesture_fixture_starts_joined",seam(startTarget,startSource)&&std::abs(startTarget)<1.e-9);
    const auto beforeMidi=midiOf("right");
    gesture("right",0,2,"pure_vertical_drag_keeps_seam_joined");
    check("pure_vertical_drag_actually_transposed",std::abs(midiOf("right")-beforeMidi-2.f)<1.e-4);
    gesture("right",.15,0,"horizontal_drag_keeps_seam_joined");
    gesture("left",-.1,-1,"combined_drag_keeps_seam_joined");
    const auto state=model.snapshot();
    check("real_gesture_path_keeps_both_owners",state.tracks[0].clips.size()==2);
    bool clocks=true;
    for(const auto& clip:state.tracks[0].clips)
    {
        const auto clock=nativeClipClock(clip);clocks&=clock.size()>=2;
        for(std::size_t i=1;i<clock.size();++i)
            clocks&=clock[i].targetSeconds>clock[i-1].targetSeconds&&clock[i].sourceSeconds>clock[i-1].sourceSeconds;
    }
    check("real_gesture_path_keeps_valid_source_clocks",clocks);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
