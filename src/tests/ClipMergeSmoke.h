#pragma once
#include "../ClipParts.h"
namespace hachi
{
inline bool MainComponent::diagnosticClipMerge(const juce::File& folder)
{
    folder.createDirectory();stopTimer();setSize(1280,800);bool ok=true;
    const auto check=[&](const char* name,bool passed){ok=ok&&passed;std::cout<<name<<'='<<passed<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<1.0e-7;};
    const auto fileA=folder.getChildFile("a.wav"),fileB=folder.getChildFile("b.wav");
    for(int n=0;n<2;++n)
    {
        const int rate=n?44100:48000,channels=n?1:2;
        juce::AudioBuffer<float> samples(channels,rate*4);
        for(int c=0;c<channels;++c)for(int i=0;i<samples.getNumSamples();++i)
        {const auto t=static_cast<double>(i)/rate;samples.setSample(c,i,static_cast<float>(.16*std::sin(juce::MathConstants<double>::twoPi*((179+n*173+c*83)*t+13*t*t))));}
        auto stream=(n?fileB:fileA).createOutputStream();stream->setPosition(0);stream->truncate();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),rate,channels,32,{},0));
        check("source_written",writer&&writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));
    }
    const auto refresh=[&]{project.dispatchPendingMessages();stopTimer();resized();timeline.setPixelsPerSecond(140);
        (void)timeline.createComponentSnapshot(timeline.getLocalBounds());};
    const auto render=[&](const juce::String& name)
    {
        audio.syncProject(project.snapshot());juce::String error;const auto file=folder.getChildFile(name+".wav");
        check("export_succeeded",audio.exportWav(file,error,"audio",0,4,{48000,2,32}));
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
        juce::AudioBuffer<float> result(2,192000);result.clear();
        check("export_length_unchanged",reader&&reader->lengthInSamples==192000);
        if(reader)reader->read(&result,0,std::min(192000,static_cast<int>(reader->lengthInSamples)),0,true,true);
        return result;
    };
    const auto difference=[](const auto& a,const auto& b,double from=0,double to=4)
    {float d=0;for(int c=0;c<2;++c)for(int i=static_cast<int>(from*48000);i<static_cast<int>(to*48000);++i)
        d=std::max(d,std::abs(a.getSample(c,i)-b.getSample(c,i)));return d;};
    const auto first=[&]{return project.snapshot().tracks[0].clips.front();};
    ProjectData preview;
    for(const bool backing:{true,false})
    {
        ProjectData data;TrackData track;track.id="audio";track.name=backing?"Merged backing":"Merged audio";
        track.accompaniment=backing;track.compose=false;track.volume=.8f;track.pan=-.2f;
        ClipData a;a.id="a";a.sourceFile=fileA;a.startSeconds=.5;a.durationSeconds=1;a.sourceOffsetSeconds=.1;
        a.sourceDurationSeconds=backing?1:1.3;a.gain=.7f;a.fadeInSeconds=.05;a.fadeOutSeconds=.04;
        NoteData na;na.id="na";na.startSeconds=.2;na.durationSeconds=.7;na.utauFlags="Mb70";
        na.diffSingerPitchReference={{0,60},{.7,63}};na.utauFlagCurves={{"g",{{0,-20},{.7,30}}}};a.notes={na};
        ClipData b=a;b.id="b";b.sourceFile=fileB;b.startSeconds=2;b.sourceOffsetSeconds=.2;
        b.sourceDurationSeconds=backing?1:.8;b.gain=.4f;b.fadeInSeconds=.02;b.fadeOutSeconds=.1;b.notes[0].id="nb";
        b.notes[0].utauFlags="Mt50";b.notes[0].diffSingerTiming="{\"phonemes\":[]}";
        ClipData c=a;c.id="c";c.startSeconds=3.5;c.durationSeconds=.5;c.notes.clear();c.sourceDurationSeconds=.5;
        track.clips={a,b,c};data.tracks={track};project.replace(data);refresh();
        const juce::String tag=backing?"backing":"raw";
        const auto before=render(tag+"-before");timeline.setSelectedClips({"a","b"});
        bool mergeEnabled=false,splitRenamed=false;auto menu=clipContextMenu("a",1);
        for(juce::PopupMenu::MenuItemIterator it(menu);it.next();)
        {if(it.getItem().itemID==2)mergeEnabled=it.getItem().isEnabled;if(it.getItem().itemID==1)splitRenamed=it.getItem().text==strings.text("clip.split");}
        check("merge_menu_enabled_for_same_track",mergeEnabled);check("split_uses_renamed_label",splitRenamed);
        const auto revision=project.revisionNumber();clipContextMenuItemChosen(2,"a",1);refresh();
        check("merge_creates_one_region_on_same_track",project.snapshot().tracks[0].clips.size()==2&&first().parts.size()==2
            &&near(first().startSeconds,.5)&&near(first().durationSeconds,2.5));
        check("merged_region_selected",timeline.selectedClipIds()==std::vector<juce::String>{"a"}&&selectedClipId=="a");
        check("notes_flags_and_predictions_preserved",first().notes.size()==2&&near(first().notes[1].startSeconds,1.7)
            &&first().notes[0].utauFlags=="Mb70"&&first().notes[1].utauFlags=="Mt50"
            &&first().notes[1].diffSingerTiming==b.notes[0].diffSingerTiming&&near(first().notes[0].diffSingerPitchReference.back().timeSeconds,.7)
            &&near(first().notes[0].utauFlagCurves[0].points.back().timeSeconds,.7));
        const auto mergedAudio=render(tag+"-merged");const auto diff=difference(before,mergedAudio);
        std::cout<<"merge_audio_max_difference="<<diff<<std::endl;
        check("different_sources_gains_fades_and_gap_sound_identical",diff<1.0e-6f);
        check("merge_one_undo_step",project.revisionNumber()==revision+1&&project.undo()&&project.snapshot().tracks[0].clips.size()==3);
        check("merge_redo",project.redo()&&first().parts.size()==2);refresh();
        juce::String error;ProjectModel reopened;const auto saved=folder.getChildFile(tag+".hjpx");
        check("save_reopen_preserves_sources_and_note_ownership",project.save(saved,error)&&reopened.load(saved,error)
            &&reopened.snapshot().tracks[0].clips[0].parts.size()==2
            &&reopened.snapshot().tracks[0].clips[0].notes[1].clipPartId==first().notes[1].clipPartId);
        project.replace(reopened.snapshot());refresh();check("reopened_audio_identical",difference(before,render(tag+"-reopened"))<1.0e-6f);
        const auto right=project.splitClip("a",1.125);check("merged_region_can_split_through_audio",right.isNotEmpty());refresh();
        check("single_source_split_returns_to_normal_clip",first().parts.empty()&&first().notes.front().clipPartId.isEmpty());
        check("split_merged_audio_stays_identical",difference(before,render(tag+"-split"))<1.0e-6f);
        check("split_parts_can_merge_again",project.mergeClips({"a",right})=="a");refresh();
        check("remerged_audio_identical",difference(before,render(tag+"-remerged"))<1.0e-6f);
        check("merged_region_can_trim",project.trimClip("a",.7,2));refresh();const auto trimmed=render(tag+"-trimmed");
        check("trim_keeps_retained_audio_timing",difference(before,trimmed,.71,2.69)<1.0e-6f);
        check("merged_region_can_extend_with_blank",project.trimClip("a",.2,3.1));refresh();
        check("extension_does_not_restore_removed_audio",difference(trimmed,render(tag+"-extended"))<1.0e-6f);
        const auto partStart=expandedClipParts(first()).front().startSeconds;
        project.moveClips({"a"},.5);
        check("group_move_shifts_all_sources",near(expandedClipParts(first()).front().startSeconds,partStart+.5));(void)project.undo();refresh();
        const auto duplicate=project.duplicateClip("a",4.5);
        check("duplicate_keeps_internal_sources",duplicate.isNotEmpty()&&project.snapshot().tracks[0].clips.back().parts.size()==first().parts.size());
        (void)project.undo();refresh();
        if(backing)preview=project.snapshot();
        for(const bool muted:{false,true})
        {
            auto overlap=data;overlap.tracks[0].clips[1].startSeconds=1;overlap.tracks[0].clips[1].muted=muted;
            project.replace(overlap);const auto ref=render(tag+(muted?"-muted-before":"-overlap-before"));
            (void)project.mergeClips({"a","b"});
            check(muted?"muted_child_stays_muted":"overlap_mix_unchanged",difference(ref,render(tag+(muted?"-muted-after":"-overlap-after")))<1.0e-6f);
        }
        project.replace(data);TrackData other;other.id="other";ClipData foreign=a;foreign.id="foreign";other.clips={foreign};
        auto cross=data;cross.tracks.push_back(other);project.replace(cross);refresh();timeline.setSelectedClips({"a","foreign"});
        const auto crossMenu=clipContextMenu("a",1);
        bool disabled=false;for(juce::PopupMenu::MenuItemIterator it(crossMenu);it.next();)
            if(it.getItem().itemID==2)disabled=!it.getItem().isEnabled;
        const auto beforeInvalid=project.revisionNumber();
        check("cross_track_and_incomplete_selections_rejected",disabled&&!project.canMergeClips({"a","foreign"})
            &&project.mergeClips({"a","foreign"}).isEmpty()&&project.mergeClips({"a"}).isEmpty()
            &&project.mergeClips({"a","missing"}).isEmpty()&&project.revisionNumber()==beforeInvalid);
    }
    project.replace(preview);refresh();timeline.setSelectedClips({"a"});
    if(auto out=folder.getChildFile("merged.png").createOutputStream())
        check("merge_preview_written",juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));
    return ok;
}
}
