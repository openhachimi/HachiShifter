#pragma once
#include "../ClipParts.h"
namespace hachi
{
inline bool MainComponent::diagnosticClipGainKnob(const juce::File& folder)
{
    folder.createDirectory(); stopTimer(); setSize(1280,800); bool ok=true;
    const auto check=[&](const char* name,bool passed){ok=ok&&passed;std::cout<<name<<'='<<passed<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.0e-6;};
    const auto source=folder.getChildFile("source.wav");
    {
        juce::AudioBuffer<float> samples(2,96000);
        for(int c=0;c<2;++c) for(int i=0;i<samples.getNumSamples();++i)
            samples.setSample(c,i,static_cast<float>(.12*std::sin(juce::MathConstants<double>::twoPi*(211+c*73)*i/48000.0)));
        auto stream=source.createOutputStream();stream->setPosition(0);stream->truncate();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,32,{},0));
        check("source_written",writer&&writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));
    }
    ProjectData data;TrackData track;track.id="audio";track.name="Clip gain";track.accompaniment=true;track.compose=false;
    ClipData a;a.id="a";a.sourceFile=source;a.startSeconds=.5;a.durationSeconds=1;a.sourceDurationSeconds=1;
    a.audioStartSeconds=.1;a.audioDurationSeconds=.8;a.fadeInSeconds=.02;a.fadeOutSeconds=.03;
    NoteData note;note.id="n";note.startSeconds=.2;note.durationSeconds=.4;note.utauFlags="Mb70";
    note.diffSingerPitchReference={{0,60},{.4,62}};a.notes={note};
    ClipData b=a;b.id="b";b.startSeconds=2;b.gain=.75f;b.notes.clear();
    track.clips={a,b};data.tracks={track};project.replace(data);
    const auto refresh=[&](int height=96)
    {
        project.dispatchPendingMessages();stopTimer();resized();timelineHorizontalZoom=140;timeline.setPixelsPerSecond(140);
        timeline.setRowHeight(static_cast<float>(height));trackList.setRowHeight(height);
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());
    };
    const auto clip=[&](const char* id)
    {for(const auto& t:project.snapshot().tracks)for(const auto& c:t.clips)if(c.id==id)return c;return ClipData{};};
    const auto event=[&](juce::Point<float> p,juce::Point<float> down,int mods=0)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),0,0,0,0,0,&timeline,&timeline,
        juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,p!=down);};
    // Actual pointer positions in the lane, independent of the knob implementation.
    const juce::Point<float> knob(88,101);
    const auto gesture=[&](juce::Point<float> p,float dy,int mods=0)
    {const auto to=p.translated(0,dy);timeline.mouseDown(event(p,p,mods));timeline.mouseDrag(event(to,p,mods));
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());timeline.mouseUp(event(to,p,mods));};
    const auto render=[&](const juce::String& name)
    {
        audio.syncProject(project.snapshot());juce::String error;const auto file=folder.getChildFile(name+".wav");
        check("export_succeeded",audio.exportWav(file,error,"audio",0,3,{48000,2,32}));
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
        juce::AudioBuffer<float> result(2,144000);result.clear();
        check("export_length_unchanged",reader&&reader->lengthInSamples==144000);
        if(reader)reader->read(&result,0,std::min(144000,static_cast<int>(reader->lengthInSamples)),0,true,true);
        return result;
    };
    const auto scaledDifference=[](const auto& before,const auto& after,float scale,double from,double to)
    {float d=0;for(int c=0;c<2;++c)for(int i=static_cast<int>(from*48000);i<static_cast<int>(to*48000);++i)
        d=std::max(d,std::abs(after.getSample(c,i)-before.getSample(c,i)*scale));return d;};
    refresh();timeline.setSelectedClips({"a","b"});
    timeline.mouseMove(event(knob,knob));
    check("knob_hover_reports_value_and_help",timeline.getTooltip().contains("0.0 dB")&&timeline.getTooltip().contains("Shift"));
    auto revision=project.revisionNumber();gesture(knob,0);refresh();
    check("click_without_drag_does_not_edit",project.revisionNumber()==revision&&near(clip("a").gain,1));
    const auto baseline=render("baseline");
    timeline.mouseDown(event(knob,knob));
    for(int i=1;i<=6;++i) timeline.mouseDrag(event(knob.translated(0,static_cast<float>(i*4)),knob));
    check("drag_preview_does_not_flood_undo",project.revisionNumber()==revision&&near(clip("a").gain,1));
    if(auto out=folder.getChildFile("drag-preview.png").createOutputStream())
        check("drag_preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));
    timeline.mouseUp(event(knob.translated(0,24),knob));refresh();
    const auto minus6=juce::Decibels::decibelsToGain(-6.0f);
    check("downward_drag_reduces_only_pointed_clip",near(clip("a").gain,minus6)&&near(clip("b").gain,.75));
    check("gain_drag_preserves_timing_fades_notes_and_flags",near(clip("a").startSeconds,.5)&&near(clip("a").durationSeconds,1)
        &&near(clip("a").audioStartSeconds,.1)&&near(clip("a").fadeInSeconds,.02)&&near(clip("a").fadeOutSeconds,.03)
        &&clip("a").notes[0].utauFlags=="Mb70"&&clip("a").notes[0].diffSingerPitchReference.size()==2);
    check("multiselection_preserved",timeline.isClipSelected("a")&&timeline.isClipSelected("b"));
    check("gesture_is_one_undo_step",project.revisionNumber()==revision+1&&project.undo()&&near(clip("a").gain,1));
    check("gain_redo",project.redo()&&near(clip("a").gain,minus6));refresh();
    const auto lowered=render("lowered");
    check("audio_gain_matches_knob_db",scaledDifference(baseline,lowered,minus6,.5,1.5)<1.0e-6f);
    check("other_region_audio_unchanged",scaledDifference(baseline,lowered,1,2,3)<1.0e-6f);
    int dialogs=0;const auto oldDialog=timeline.onClipGainRequested;timeline.onClipGainRequested=[&](const auto&){++dialogs;};
    timeline.mouseDoubleClick(event(knob,knob));refresh();
    check("double_click_resets_without_dialog",dialogs==0&&near(clip("a").gain,1));
    const juce::Point<float> body(150,78);timeline.mouseDoubleClick(event(body,body));
    check("body_double_click_still_opens_gain_dialog",dialogs==1);timeline.onClipGainRequested=oldDialog;
    revision=project.revisionNumber();gesture(knob,40,juce::ModifierKeys::shiftModifier);refresh();
    check("shift_drag_fine_adjustment",near(clip("a").gain,juce::Decibels::decibelsToGain(-1.0f))&&project.revisionNumber()==revision+1);
    // Press/release Shift during the same gesture without jumping the current value.
    timeline.mouseDown(event(knob,knob));timeline.mouseDrag(event(knob.translated(0,-4),knob));
    timeline.mouseDrag(event(knob.translated(0,-8),knob,juce::ModifierKeys::shiftModifier));
    timeline.mouseUp(event(knob.translated(0,-8),knob));refresh();
    check("shift_transition_continuous",near(clip("a").gain,juce::Decibels::decibelsToGain(.1f)));
    revision=project.revisionNumber();const auto oldGain=clip("a").gain;
    timeline.mouseDown(event(knob,knob));timeline.mouseDrag(event(knob.translated(0,80),knob));
    (void)timeline.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));timeline.mouseUp(event(knob.translated(0,80),knob));refresh();
    check("escape_cancels_gain_gesture",project.revisionNumber()==revision&&near(clip("a").gain,oldGain));
    gesture(knob,-1000);refresh();check("upper_gain_limit",near(clip("a").gain,juce::Decibels::decibelsToGain(12.0f)));
    gesture(knob,1000);refresh();check("lower_gain_limit_is_silence",clip("a").gain==0&&!clip("a").muted);
    const auto silent=render("silent");check("zero_gain_mutes_audio",silent.getMagnitude(0,24000,48000)<1.0e-8f);
    gesture(knob,-40);refresh();check("can_raise_gain_from_silence",near(clip("a").gain,juce::Decibels::decibelsToGain(-50.0f)));
    project.replace(data);refresh();
    // The left half of the knob overlaps the generous edge hit strip.
    gesture({80,101},24);refresh();check("knob_takes_priority_over_edge_trim",near(clip("a").gain,minus6)&&near(clip("a").startSeconds,.5));
    project.replace(data);refresh(40);gesture({96,49},24);refresh(40);
    check("compact_lane_knob_works_without_muting",near(clip("a").gain,minus6)&&!clip("a").muted&&near(clip("a").startSeconds,.5));
    if(auto out=folder.getChildFile("compact.png").createOutputStream())
        check("compact_preview_written",juce::PNGImageFormat().writeImageToStream(timeline.createComponentSnapshot(timeline.getLocalBounds()),*out));
    auto narrow=data;narrow.tracks[0].clips[0].durationSeconds=.25;project.replace(narrow);refresh();gesture(knob,24);refresh();
    check("narrow_region_knob_works",near(clip("a").gain,minus6)&&near(clip("a").durationSeconds,.25));
    for(bool backing:{true,false})
    {
        auto group=data;group.tracks[0].accompaniment=backing;group.tracks[0].clips[0].gain=.25f;
        project.replace(group);check("merged_region_created",project.mergeClips({"a","b"})=="a");refresh();
        const auto before=render(backing?"merged-backing-before":"merged-raw-before");gesture(knob,24);refresh();
        const auto parts=expandedClipParts(clip("a"));
        check("merged_knob_preserves_relative_source_gains",parts.size()==2&&near(parts[0].gain,.25*minus6)&&near(parts[1].gain,.75*minus6));
        const auto after=render(backing?"merged-backing-after":"merged-raw-after");
        check("merged_audio_gain_applied_to_all_parts",scaledDifference(before,after,minus6,0,3)<1.0e-6f);
        juce::String error;ProjectModel reopened;const auto saved=folder.getChildFile(backing?"backing.hjpx":"raw.hjpx");
        check("save_reopen_retains_gain",project.save(saved,error)&&reopened.load(saved,error)
            &&near(reopened.snapshot().tracks[0].clips[0].gain,minus6));
        project.replace(reopened.snapshot());refresh();
        check("reopened_audio_preserves_gain",scaledDifference(after,render(backing?"backing-reopened":"raw-reopened"),1,0,3)<1.0e-6f);
    }
    project.replace(data);refresh();gesture(knob,24);refresh();timeline.setSelectedClips({"a"});
    if(auto out=folder.getChildFile("knobs.png").createOutputStream())
        check("final_preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));
    // Render the actual zoom range for visual review of the compact gain control.
    for (const int row : {48, 64, 144, 192})
    {
        refresh(row);
        if (auto out=folder.getChildFile("gain-height-"+juce::String(row)+".png").createOutputStream())
            (void)juce::PNGImageFormat().writeImageToStream(timeline.createComponentSnapshot(timeline.getLocalBounds()),*out);
    }
    return ok;
}
}
