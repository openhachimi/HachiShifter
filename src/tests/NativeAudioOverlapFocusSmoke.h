#pragma once
#include "../NativeAudioFocus.h"
namespace hachi
{
inline bool runNativeAudioOverlapFocusSmoke(const juce::File& folder)
{
    folder.createDirectory();if(!runNativeAudioOverlapSmoke(folder.getChildFile("seed")))return false;
    bool ok=true;int checks=0;const auto check=[&](const char* key,bool pass){ok &= pass;++checks;std::cout<<key<<'='<<pass<<std::endl;};
    ProjectModel model;juce::String error;if(!model.load(folder.getChildFile("seed/overlap-enabled.hjpx"),error))return false;
    const auto before=model.snapshot();auto fixture=before;auto& track=fixture.tracks[0];auto neighbor=track.clips.front();neighbor.id="C";neighbor.startSeconds=4;neighbor.notes[0].id="C-note";track.clips.push_back(neighbor);model.replace(fixture);
    check("chosen_source_stays_bright",nativeOverlapFocusOpacity(track,track.clips[0],"A-note",{})==1.f);
    check("other_overlapping_source_dims",nativeOverlapFocusOpacity(track,track.clips[1],"A-note",{})<.3f);
    check("nonoverlapping_source_keeps_brightness",nativeOverlapFocusOpacity(track,track.clips[2],"A-note",{})==1.f);
    check("switching_source_reverses_focus",nativeOverlapFocusOpacity(track,track.clips[0],"B-note",{})<.3f&&nativeOverlapFocusOpacity(track,track.clips[1],"B-note",{})==1.f);
    check("empty_selection_has_no_dimming",nativeOverlapFocusOpacity(track,track.clips[0],{},{})==1.f&&nativeOverlapFocusOpacity(track,track.clips[1],{},{})==1.f);
    auto u=track;u.pitchAlgorithm=PitchAlgorithm::utau;check("utau_does_not_dim_sources",nativeOverlapFocusOpacity(u,u.clips[1],"A-note",{})==1.f);
    const auto order=nativeAudioFocusOrder(track,"A-note",{});check("focused_source_paints_above_dimmed_source",order.front()->id=="B");
    auto linked=track;linked.clips={assembledLinkedAudio({track.clips[0],track.clips[1]})};ProjectData expanded;expanded.tracks={linked};expandProjectClipParts(expanded,true);
    const auto& children=expanded.tracks[0];check("linked_children_with_same_parent_id_focus_independently",children.clips[0].id==children.clips[1].id
        &&nativeOverlapFocusOpacity(children,children.clips[0],"A-note",children.clips[0].id)==1.f
        &&nativeOverlapFocusOpacity(children,children.clips[1],"A-note",children.clips[0].id)<.3f);
    const auto selectionRevision=model.revisionNumber();
    I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setFocusedClip("A");roll.setTool(PianoRollComponent::Tool::note);
    roll.setPixelsPerSecond(300);roll.setRowHeight(24);roll.setSize(1700,roll.getHeight());roll.diagnosticRefresh();juce::Thread::sleep(200);
    const auto y=static_cast<int>(roll.diagnosticYForMidi(57))-160;
    const auto capture=[&](const char* name){juce::Image image(juce::Image::RGB,1100,300,true,juce::SoftwareImageType{});
        {juce::Graphics g(image);g.fillAll(Palette::background);g.setOrigin(0,-y);roll.paint(g);}
        auto out=folder.getChildFile(name).createOutputStream();if(out){out->setPosition(0);out->truncate();juce::PNGImageFormat().writeImageToStream(image,*out);}return image;};
    roll.setSelectedNoteIds({"A-note"});const auto first=capture("focus-earlier.png");
    const auto event=[](juce::Component& c,juce::Point<float> p){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys::leftButtonModifier,0,0,0,0,0,
        &c,&c,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    const auto later=roll.diagnosticOverlapSourceBounds("B-note");check("overlap_card_exposes_later_source_selector",!later.isEmpty());
    roll.mouseDown(event(roll,later.getCentre()));roll.mouseUp(event(roll,later.getCentre()));check("clicking_later_card_selects_occluded_audio",roll.selectedNoteIds().size()==1&&roll.selectedNoteIds()[0]=="B-note");
    const auto second=capture("focus-later.png");
    const auto brightness=[](const juce::Image& image,int x){float result=0;for(int y=130;y<190;++y)for(int dx=-3;dx<=3;++dx)result=std::max(result,image.getPixelAt(x+dx,y).getBrightness());return result;};
    check("earlier_ink_dims_when_later_is_clicked",brightness(first,420)>brightness(second,420)+.12f);
    check("later_ink_brightens_when_later_is_clicked",brightness(second,748)>brightness(first,748)+.12f);
    const auto earlier=roll.diagnosticOverlapSourceBounds("A-note");roll.mouseDown(event(roll,earlier.getCentre()));roll.mouseUp(event(roll,earlier.getCentre()));check("clicking_earlier_card_switches_focus_back",roll.selectedNoteIds()[0]=="A-note");
    // The main window refocuses in response to a click, rebuilding hit regions.
    // Ensure that reordering does not make the following drag grab the peer.
    roll.onNoteSelected=[&](const auto& id){roll.setFocusedClip(id=="A-note"?"A":"B");};
    const auto shared=juce::Point<float>(58+1.8f*300,roll.diagnosticYForMidi(57));roll.mouseDown(event(roll,shared));roll.mouseUp(event(roll,shared));
    check("overlap_body_hit_matches_foreground_source",roll.selectedNoteIds()[0]=="A-note");
    check("selection_does_not_edit_project_or_create_undo",model.revisionNumber()==selectionRevision&&model.snapshot().tracks[0].clips.size()==3
        &&model.snapshot().tracks[0].clips[0].startSeconds==fixture.tracks[0].clips[0].startSeconds&&model.snapshot().tracks[0].clips[1].durationSeconds==fixture.tracks[0].clips[1].durationSeconds);
    TimelineComponent lane(model);lane.setPixelsPerSecond(300);lane.setRowHeight(160);lane.setSize(1700,lane.getHeight());lane.diagnosticRefresh();
    const auto laneCapture=[&](const char* name){juce::Image image(juce::Image::RGB,1100,210,true,juce::SoftwareImageType{});{juce::Graphics g(image);g.fillAll(Palette::background);lane.paint(g);}
        auto out=folder.getChildFile(name).createOutputStream();if(out){out->setPosition(0);out->truncate();juce::PNGImageFormat().writeImageToStream(image,*out);}return image;};
    lane.setSelectedClips({"A"});const auto topA=laneCapture("timeline-earlier.png");lane.setSelectedClips({"B"});const auto topB=laneCapture("timeline-later.png");
    const auto topBrightness=[](const juce::Image& image,int x){float result=0;for(int y=70;y<175;++y)for(int dx=-3;dx<=3;++dx)result=std::max(result,image.getPixelAt(x+dx,y).getBrightness());return result;};
    check("timeline_peer_dims_when_selection_switches",topBrightness(topA,360)>topBrightness(topB,360)+.1f&&topBrightness(topB,690)>topBrightness(topA,690)+.1f);
    check("focus_does_not_disable_overlap",model.snapshot().tracks[0].allowNativeAudioOverlap);
    roll.mouseDoubleClick(event(roll,earlier.getCentre()));check("double_clicking_card_does_not_flatten_audio",model.revisionNumber()==selectionRevision);
    MainComponent main;main.setSize(1600,900);main.diagnosticProject().replace(fixture);main.diagnosticProject().dispatchPendingMessages();main.diagnosticSelectTrack("native");
    main.diagnosticSelectNotes({"A-note"});check("lower_note_selection_updates_timeline_display_focus",main.diagnosticTimeline().diagnosticNativeFocusedNote()=="A-note");
    const auto mainRevision=main.diagnosticProject().revisionNumber();main.diagnosticTimeline().setSelectedClips({"B"});
    check("timeline_click_replaces_old_lower_note_focus",main.diagnosticPianoRoll().selectedNoteIds().empty()&&main.diagnosticTimeline().diagnosticNativeFocusedNote().isEmpty());
    main.diagnosticSelectNotes({"A-note"});check("editing_visible_peer_updates_display_without_hiding_regions",main.diagnosticTimeline().diagnosticNativeFocusedNote()=="A-note"
        &&main.diagnosticPianoRoll().diagnosticHitCount()==3&&main.diagnosticProject().revisionNumber()==mainRevision);

    std::cout<<"native_audio_overlap_focus_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
