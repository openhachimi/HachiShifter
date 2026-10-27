#pragma once
#include "../NativeCoupledSelectionMove.h"
#include "../ProjectModel.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativePermanentSeamSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" | "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto wave=folder.getChildFile("phrase.wav");
    {juce::AudioBuffer<float> pcm(1,240000);for(int s=0;s<pcm.getNumSamples();++s)pcm.setSample(0,s,float(.08*std::sin(s*juce::MathConstants<double>::twoPi*220/48000.)));auto stream=wave.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
    struct Extent{double from=0,to=0;};
    const auto extents=[](const ProjectData& data){std::array<Extent,3> out;for(const auto& track:data.tracks)for(const auto& clip:track.clips)for(const auto& note:clip.notes)for(int i=0;i<3;++i)if(note.id==juce::String::charToString(juce::juce_wchar('a'+i)))out[std::size_t(i)]={clip.startSeconds+note.startSeconds,clip.startSeconds+note.startSeconds+note.durationSeconds};return out;};
    const auto joined=[&](const ProjectData& data){const auto x=extents(data);return std::abs(x[0].to-x[1].from)<1.e-7&&std::abs(x[1].to-x[2].from)<1.e-7&&x[0].to>x[0].from&&x[1].to>x[1].from&&x[2].to>x[2].from;};
    for(int mode=0;mode<4;++mode)
    {
        const auto tag=juce::String(mode==0?"single-owner-partial":mode==1?"split-owner-partial":mode==2?"virtual-normal":"virtual-transition");
        ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;
        ClipData combined;combined.id="phrase";combined.sourceFile=wave;combined.startSeconds=1;combined.durationSeconds=combined.sourceDurationSeconds=3;combined.sourceTimeMap={{0,0},{3,3}};
        for(int i=0;i<3;++i)
        {
            ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=wave;clip.startSeconds=1+i;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceOffsetSeconds=mode==1?i:0;clip.sourceTimeMap={{0,0},{1,1}};
            if(mode>=2){clip.sourceFile=folder.getChildFile("virtual"+juce::String(i)+".wav");if(!clip.sourceFile.existsAsFile()&&!wave.copyFileTo(clip.sourceFile))return false;}
            if(mode==1){clip.materialSourceUsageGroupId="partial-use";clip.glideConnectedFromPrevious=i>0;clip.glideConnectedToNext=i<2;}
            NoteData note;note.id=juce::String::charToString(juce::juce_wchar('a'+i));note.startSeconds=mode==0?i:0;note.durationSeconds=1;note.midiNote=note.sourceMidiCenter=60+float(i);note.sourcePitchMeasured=true;
            if(mode<2){note.connectedToPrevious=i>0;note.connectedToNext=i<2;}
            for(int f=0;f<=200;++f)note.contour.push_back({f*.005,0,0,true});
            if(mode==3&&i==1){note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=NativeTransitionShape{1,.5,"explicit-test-middle"};}
            if(mode==0)combined.notes.push_back(note);else{clip.notes={note};track.clips.push_back(clip);}
        }
        if(mode==0)track.clips={combined};seed.tracks={track};ProjectModel model;model.resetDocument(seed);
        if(mode>=2){check(tag+"_bind",model.setNativeVirtualConnectionForNotes({"a","b","c"},true,false,error));seed=model.snapshot();}
        const auto before=model.contentFingerprint();
        struct Edit{const char* id;NativeNoteTimeEdit kind;double delta;};
        const std::vector<Edit> edits{{"b",NativeNoteTimeEdit::move,-.2},{"b",NativeNoteTimeEdit::move,.2},{"b",NativeNoteTimeEdit::leftEdge,-.2},{"b",NativeNoteTimeEdit::leftEdge,.2},{"b",NativeNoteTimeEdit::rightEdge,-.2},{"b",NativeNoteTimeEdit::rightEdge,.2},{"a",NativeNoteTimeEdit::move,.2},{"a",NativeNoteTimeEdit::move,-.2},{"c",NativeNoteTimeEdit::move,.2},{"c",NativeNoteTimeEdit::move,-.2},{"a",NativeNoteTimeEdit::rightEdge,.2},{"a",NativeNoteTimeEdit::rightEdge,-.2},{"c",NativeNoteTimeEdit::leftEdge,.2},{"c",NativeNoteTimeEdit::leftEdge,-.2}};
        int test=0;
        for(const auto& edit:edits)
        {
            model.resetDocument(seed);const auto original=model.contentFingerprint();
            const bool changed=edit.kind==NativeNoteTimeEdit::move?model.moveNativeNotes({edit.id},edit.delta,0,&error):model.resizeNativeNoteEdge(edit.id,edit.delta,edit.kind==NativeNoteTimeEdit::leftEdge);
            const auto state=model.snapshot();const auto x=extents(state);bool expected=true;
            if(juce::String(edit.id)=="b")
            {
                const double start=2+(edit.kind!=NativeNoteTimeEdit::rightEdge?edit.delta:0),end=3+(edit.kind!=NativeNoteTimeEdit::leftEdge?edit.delta:0);
                expected=std::abs(x[0].from-1)<1.e-7&&std::abs(x[0].to-start)<1.e-7&&std::abs(x[1].from-start)<1.e-7&&std::abs(x[1].to-end)<1.e-7&&std::abs(x[2].from-end)<1.e-7&&std::abs(x[2].to-4)<1.e-7;
            }
            if(juce::String(edit.id)=="a"&&edit.kind==NativeNoteTimeEdit::rightEdge)expected=std::abs(x[0].to-(2+edit.delta))<1.e-7&&std::abs(x[1].from-(2+edit.delta))<1.e-7&&std::abs(x[1].to-3)<1.e-7;
            if(juce::String(edit.id)=="c"&&edit.kind==NativeNoteTimeEdit::leftEdge)expected=std::abs(x[1].to-(3+edit.delta))<1.e-7&&std::abs(x[2].from-(3+edit.delta))<1.e-7&&std::abs(x[1].from-2)<1.e-7;
            check(tag+"_gesture_"+juce::String(++test),changed&&joined(state)&&expected);
            if(!changed||!joined(state)||!expected)log<<"actual="<<x[0].from<<".."<<x[0].to<<" / "<<x[1].from<<".."<<x[1].to<<" / "<<x[2].from<<".."<<x[2].to<<"\n";
            const auto applied=model.contentFingerprint();model.undo();check(tag+"_undo_"+juce::String(test),model.contentFingerprint()==original);model.redo();check(tag+"_redo_"+juce::String(test),model.contentFingerprint()==applied);
        }
        {
            I18n strings;PianoRollComponent roll(model,strings);roll.setFocusedTrack("native");roll.setTool(PianoRollComponent::Tool::note);roll.setShowNativeWaveforms(false);roll.setPixelsPerSecond(400);roll.setRowHeight(26);roll.setSize(2200,3600);
            const auto paint=[&]{juce::Image image(juce::Image::ARGB,2100,400,true,juce::SoftwareImageType{});juce::Graphics graphics(image);graphics.addTransform(juce::AffineTransform::translation(0,-float(roll.diagnosticYForMidi(64))+20));roll.paintEntireComponent(graphics,true);};
            const auto event=[&](juce::Point<float> at,juce::Point<float> down){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::altModifier),1,0,0,0,0,&roll,&roll,juce::Time::getCurrentTime(),down,juce::Time::getCurrentTime(),1,at!=down);};
            int gesture=0;
            for(const auto& edit:edits)
            {
                model.resetDocument(seed);model.dispatchPendingMessages();roll.diagnosticRefresh();paint();
                std::optional<juce::Rectangle<float>> bounds;
                for(std::size_t hit=0;hit<roll.diagnosticHitCount();++hit)if(roll.diagnosticHitNoteId(hit)==edit.id)bounds=roll.diagnosticHitNominalBounds(hit);
                bool moved=false;
                if(bounds)
                {
                    const auto down=edit.kind==NativeNoteTimeEdit::leftEdge?juce::Point<float>(bounds->getX()+2,bounds->getCentreY()):edit.kind==NativeNoteTimeEdit::rightEdge?juce::Point<float>(bounds->getRight()-2,bounds->getCentreY()):bounds->getCentre();
                    const auto up=down.translated(float(edit.delta*400),0);const auto fp=model.contentFingerprint();roll.mouseDown(event(down,down));roll.mouseDrag(event(down+(up-down)*.5f,down));roll.mouseDrag(event(up,down));paint();
                    check(tag+"_mouse_preview_no_early_commit_"+juce::String(gesture+1),model.contentFingerprint()==fp);
                    roll.mouseUp(event(up,down));model.dispatchPendingMessages();roll.diagnosticRefresh();moved=model.contentFingerprint()!=fp;
                }
                check(tag+"_real_mouse_seam_"+juce::String(++gesture),bounds&&moved&&joined(model.snapshot()));
            }
            for(int direction:{-1,1})
            {
                model.resetDocument(seed);model.dispatchPendingMessages();roll.diagnosticRefresh();paint();std::optional<juce::Point<float>> down;
                for(std::size_t hit=0;hit<roll.diagnosticHitCount();++hit)if(roll.diagnosticHitNoteId(hit)=="b")down=roll.diagnosticHitNominalBounds(hit).getCentre();
                const auto fp=model.contentFingerprint();if(down){const auto up=down->translated(0,float(-direction*26));roll.mouseDown(event(*down,*down));roll.mouseDrag(event(up,*down));roll.mouseUp(event(up,*down));}
                check(tag+"_vertical_mouse_"+juce::String(direction),down&&joined(model.snapshot())&&model.contentFingerprint()!=fp);
            }
        }
        model.resetDocument(seed);const bool limited=model.moveNativeNotes({"b"},5,0,&error);check(tag+"_large_drag_clamps_without_tearing",limited&&joined(model.snapshot())&&extents(model.snapshot())[1].to<=3.991);
        const auto file=folder.getChildFile(tag+".hjpx");ProjectModel cold;check(tag+"_cold_seams",model.save(file,error)&&cold.load(file,error)&&joined(cold.snapshot()));
        if(mode>=2)check(tag+"_exact_binding_order_retained",model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==std::vector<juce::String>{"a","b","c"});
        model.resetDocument(seed);check(tag+"_moving_outer_pair_carries_middle",model.moveNativeNotes({"a","c"},.2,0,&error)&&joined(model.snapshot())&&std::abs(extents(model.snapshot())[1].from-2.2)<1.e-7&&std::abs(extents(model.snapshot())[1].to-3.2)<1.e-7);
        model.resetDocument(seed);model.resizeNativeNoteEdge("b",5,true);const auto atLimit=model.contentFingerprint();const bool more=model.resizeNativeNoteEdge("b",.2,true);check(tag+"_minimum_length_does_not_fall_back_to_a_free_edit",!more&&model.contentFingerprint()==atLimit&&joined(model.snapshot()));
        check(tag+"_crop_cannot_implicitly_disconnect",!model.trimNativeNoteEdge("b",.05,true)&&model.contentFingerprint()==atLimit);
        if(mode>=2){auto legacy=seed;legacy.nativeVirtualGroups[0].coupledMovement=false;model.resetDocument(legacy);check(tag+"_legacy_free_flag_cannot_tear_shared_boundaries",model.moveNativeNotes({"b"},.2,0,&error)&&joined(model.snapshot()));}
        if(mode==3)check("typed_source_middle_remains_source_time",std::abs(model.snapshot().tracks[0].clips[1].notes[0].nativeTransition->boundarySourceSeconds-.5)<1.e-9);
    }
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
