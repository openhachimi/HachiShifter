#pragma once
namespace hachi
{
inline bool MainComponent::diagnosticTimelineNotes(const juce::File& folder)
{
    folder.createDirectory();stopTimer();bool ok=true;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;};
    ProjectData data;
    for(int row=0;row<5;++row)
    {
        TrackData track;track.id="t"+juce::String(row);track.name="Preview "+juce::String(row);
        track.compose=row<3;track.accompaniment=row==3;
        if(row<2){track.pitchAlgorithm=PitchAlgorithm::utau;track.utauMode=row==0?UtauMode::classic:UtauMode::mou;}
        ClipData clip;clip.id="c"+juce::String(row);clip.startSeconds=1;clip.durationSeconds=3;
        clip.sourceFile=folder.getChildFile("no-rendered-audio-"+juce::String(row)+".mid");
        // Notes are target-local even if the underlying audio body/source offset differs.
        clip.audioStartSeconds=.25;clip.audioDurationSeconds=2.5;clip.sourceOffsetSeconds=7;
        for(int n=0;n<2;++n)
        {NoteData note;note.id="n"+juce::String(row)+"-"+juce::String(n);note.label=n?"hao":"ni";
            note.startSeconds=.4+n;note.durationSeconds=.5;note.midiNote=n?72:60;clip.notes.push_back(note);}
        track.clips={clip};data.tracks.push_back(track);
    }
    ProjectModel previewModel;previewModel.replace(data);TimelineComponent lane(previewModel);
    float zoom=140;int height=96;
    const auto refresh=[&]{previewModel.dispatchPendingMessages();lane.setPixelsPerSecond(zoom);lane.setRowHeight(static_cast<float>(height));};
    const auto capture=[&]{return lane.createComponentSnapshot({0,0,1100,lane.getHeight()});};
    const auto blankFor=[&](ProjectData blank)
    {
        for(auto& track:blank.tracks)for(auto& clip:track.clips)clip.notes.clear();
        ProjectModel blankModel;blankModel.replace(blank);TimelineComponent blankLane(blankModel);
        blankLane.setPixelsPerSecond(zoom);blankLane.setRowHeight(static_cast<float>(height));
        blankLane.setSelectedClips(lane.selectedClipIds(),false);
        return blankLane.createComponentSnapshot({0,0,1100,lane.getHeight()});
    };
    const auto extent=[](const juce::Image& picture,const juce::Image& blank,juce::Rectangle<int> area)
    {
        juce::Rectangle<int> result;
        for(int y=area.getY();y<area.getBottom();++y)for(int x=area.getX();x<area.getRight();++x)
            if(picture.getPixelAt(x,y)!=blank.getPixelAt(x,y))
                result=result.getUnion({x,y,1,1});
        return result;
    };
    const auto event=[&](juce::Point<float> p,juce::Point<float> down)
    {return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&lane,&lane,
        juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,p!=down);};
    const auto save=[&](const juce::String& name,const juce::Image& picture)
    {auto out=folder.getChildFile(name+".png").createOutputStream();check("preview_written",out&&juce::PNGImageFormat().writeImageToStream(picture,*out));};
    refresh();auto blank=blankFor(data);auto picture=capture();save("notes",picture);
    const auto first=extent(picture,blank,{190,63,85,25}),second=extent(picture,blank,{330,63,85,25});
    check("notes_visible_without_source_or_render",!first.isEmpty()&&!second.isEmpty());
    check("note_start_and_duration_align_with_timeline",first.getX()==196&&first.getRight()==266&&second.getX()==336&&second.getRight()==406);
    check("higher_pitch_drawn_above_lower_pitch",second.getCentreY()<first.getCentreY());
    for(int row=1;row<3;++row)
    {
        const auto same=extent(picture,blank,{190,63+row*96,85,25});
        check("utau_and_native_tracks_share_pitch_scale",same.getX()==first.getX()&&same.getY()-row*96==first.getY());
    }
    check("backing_and_raw_tracks_do_not_draw_notes",extent(picture,blank,{155,63+3*96,380,25}).isEmpty()
        &&extent(picture,blank,{155,63+4*96,380,25}).isEmpty());
    check("rests_stay_empty",extent(picture,blank,{275,63,45,25}).isEmpty());
    previewModel.transposeNote("n0-0",4);refresh();picture=capture();
    check("pitch_edit_updates_upper_preview",extent(picture,blank,{190,63,85,25}).getCentreY()<first.getCentreY());
    previewModel.resizeNote("n0-0",.6,.75);refresh();picture=capture();
    const auto resized=extent(picture,blank,{215,63,115,25});
    check("note_timing_edit_updates_upper_preview",resized.getX()==224&&resized.getRight()==329);
    previewModel.replace(data);refresh();lane.setSelectedClips({"c0"});
    const juce::Point<float> down(300,75),moved(440,75);lane.mouseDown(event(down,down));lane.mouseDrag(event(moved,down));
    auto expected=data;expected.tracks[0].clips[0].startSeconds=2;blank=blankFor(expected);picture=capture();
    check("move_preview_shifts_notes_by_clip_delta",extent(picture,blank,{330,63,85,25}).getX()==336);
    lane.mouseUp(event(moved,down));refresh();picture=capture();
    check("move_commit_matches_preview",extent(picture,blank,{330,63,85,25}).getX()==336);
    previewModel.replace(data);refresh();
    const juce::Point<float> edge(143,78),trimmed(213,78);lane.mouseDown(event(edge,edge));lane.mouseDrag(event(trimmed,edge));
    expected=data;expected.tracks[0].clips[0].startSeconds=1.5;expected.tracks[0].clips[0].durationSeconds=2.5;
    // For the blank reference keep the source/audio body at its original absolute position.
    expected.tracks[0].clips[0].audioStartSeconds=-.25;blank=blankFor(expected);picture=capture();
    const auto trimFirst=extent(picture,blank,{218,63,56,25});
    check("trim_preview_preserves_note_end",trimFirst.getRight()==266);
    check("trim_preview_does_not_shift_later_notes",extent(picture,blank,{330,63,85,25}).getX()==336);
    lane.mouseUp(event(trimmed,edge));refresh();blank=blankFor(previewModel.snapshot());picture=capture();
    check("trim_commit_preserves_note_end",extent(picture,blank,{218,63,56,25}).getRight()==266);
    check("trim_can_extend_with_silence",previewModel.trimClip("c0",.5,3.5));refresh();blank=blankFor(previewModel.snapshot());picture=capture();
    check("extended_region_does_not_stretch_notes",extent(picture,blank,{330,63,85,25}).getX()==336);
    check("extended_region_stays_blank",extent(picture,blank,{95,63,65,25}).isEmpty());
    previewModel.replace(data);refresh();const auto right=previewModel.splitClip("c0",2);refresh();
    blank=blankFor(previewModel.snapshot());picture=capture();
    check("split_preserves_note_timeline_positions",right.isNotEmpty()&&extent(picture,blank,{330,63,85,25}).getX()==336);
    check("merge_split_regions",previewModel.mergeClips({"c0",right})=="c0");refresh();blank=blankFor(previewModel.snapshot());picture=capture();
    check("merged_note_offsets_not_applied_twice",extent(picture,blank,{190,63,85,25}).getX()==196&&extent(picture,blank,{330,63,85,25}).getX()==336);
    juce::String error;const auto file=folder.getChildFile("saved.hjpx");ProjectModel reopened;
    check("save_reopen_succeeds",previewModel.save(file,error)&&reopened.load(file,error));previewModel.replace(reopened.snapshot());refresh();blank=blankFor(previewModel.snapshot());picture=capture();
    check("save_reopen_retains_note_preview",extent(picture,blank,{330,63,85,25}).getX()==336);
    previewModel.replace(data);refresh();zoom=280;refresh();blank=blankFor(data);picture=capture();
    const auto zoomed=extent(picture,blank,{385,63,155,25});check("horizontal_zoom_scales_note_start_and_length",zoomed.getX()==392&&zoomed.getRight()==532);
    zoom=140;height=120;refresh();blank=blankFor(data);picture=capture();
    check("vertical_zoom_keeps_notes_visible",!extent(picture,blank,{190,63,85,49}).isEmpty());save("tall-notes",picture);
    // Optional real project copy: inspect it without rendering or modifying the user's project.
    const auto realFile=folder.getChildFile("preview-input.hjpx");
    if(realFile.existsAsFile())
    {
        ProjectModel real;check("real_project_loads",real.load(realFile,error));
        const auto realData=real.snapshot();int count=0;
        for(const auto& track:realData.tracks)if(track.compose&&!track.accompaniment)for(const auto& clip:track.clips)count+=static_cast<int>(clip.notes.size());
        std::cout<<"real_project_note_count="<<count<<std::endl;check("real_project_contains_note_tracks",count>0);
        previewModel.replace(realData);zoom=40;height=120;refresh();save("real-project",capture());
    }
    return ok;
}
}
