#pragma once
#include "../MainComponent.h"
#include "../DiffSingerParameterCurves.h"
#include "../backend/DiffSingerRenderer.h"
#include <iostream>
namespace hachi {
inline void MainComponent::diagnosticParameterRetake(const juce::File& output,
    const juce::File& bank, std::function<void(bool)> done)
{
    output.createDirectory();stopTimer();setSize(1400,860);
    struct Report {bool ok=true;juce::Array<juce::var> checks;};
    const auto report=std::make_shared<Report>();
    const auto check=[report](const char* name,bool ok) {
        report->ok=report->ok&&ok;auto* c=new juce::DynamicObject();c->setProperty("name",name);c->setProperty("passed",ok);
        report->checks.add(c);std::cout<<name<<'='<<ok<<std::endl;
    };
    const auto finish=[report,output,done] {
        auto* r=new juce::DynamicObject();r->setProperty("ok",report->ok);r->setProperty("checks",report->checks);
        output.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(r),true));done(report->ok);
    };
    struct Await final:juce::Timer {
        int remaining=1200;std::function<bool()> ready;std::function<void()> next,fail;
        Await(std::function<bool()> r,std::function<void()> n,std::function<void()> f):ready(r),next(n),fail(f){startTimer(100);}
        void timerCallback() override {
            if(ready()){stopTimer();auto f=std::move(next);delete this;f();}
            else if(--remaining<=0){stopTimer();auto f=std::move(fail);delete this;f();}
        }
    };
    const auto wait=[this,check,finish](std::function<void()> next) {
        new Await([this]{return !diffSingerBusy;},std::move(next),[this,check,finish]{
            if(diffSingerCancel)diffSingerCancel->store(true);check("retake_completed",false);finish();});
    };
    backend::DiffSingerOptions opts;opts.backend=2;opts.preview=1;backend::DiffSingerRenderer::configure(opts);
    const auto capabilities=juce::JSON::parse(R"([{"key":"DS:DYN","supported":true},{"key":"DS:BREC","supported":true},{"key":"DS:TENC","supported":true},{"key":"DS:VOIC","supported":true}])");
    diffSingerCapabilities[bank.getFullPathName()]=capabilities;
    ProjectData data;TrackData t;t.id="ds";t.compose=true;t.pitchAlgorithm=PitchAlgorithm::utau;
    t.utauMode=UtauMode::mou;t.voicebankDirectory=bank;
    ClipData c;c.id="clip";c.durationSeconds=2.4;
    for(int i=0;i<2;++i){
        NoteData n;n.id="n"+juce::String(i);n.label=i==0?"ni":"hao";n.startSeconds=.5+.7*i;n.durationSeconds=.7;
        n.midiNote=60.0f+static_cast<float>(i*2);n.utauFlagCurveEnabled=false;n.utauFlagCurves.push_back({"DS:BREC",{{0,50},{.7,30}}});
        for(const auto& code:{juce::String("BREC"),juce::String("TENC"),juce::String("VOIC")})
            n.utauFlagCurves.push_back({"DS:REF:"+code,{{-.2,code=="TENC"?1.f:-23.f},{.7,code=="TENC"?1.f:-23.f}}});
        c.notes.push_back(n);
    }
    t.clips.push_back(c);data.tracks.push_back(t);project.replace(data);project.dispatchPendingMessages();
    diagnosticSelectTrack("ds");pianoRoll.setFocusedClip("clip");pianoRoll.diagnosticRefresh();pianoRoll.setSelectedNoteIds({"n1"});
    const auto curves=[this](int index){
        juce::String encoded;const auto snapshot=project.snapshot();
        for(const auto& curve:snapshot.tracks[0].clips[0].notes[index].utauFlagCurves){
            encoded+=curve.flag+":";for(const auto& p:curve.points)encoded+=juce::String(p.timeSeconds,9)+","+juce::String(p.value,9)+";";
        }return encoded;
    };
    const auto before0=curves(0),before1=curves(1);
    refreshSelectedNoteParameter();
    check("DS_button_named_FLAG",flagCurveButton.getButtonText()=="FLAG");
    const auto openRevision=project.revisionNumber();flagCurveButton.onClick();
    check("FLAG_click_opens_actual_without_enabling_offsets",pianoRoll.flagLaneFlag()=="DS:ABS:BREC"
        &&pianoRoll.currentTool()==PianoRollComponent::Tool::flagCurve&&!diffSingerBusy
        &&project.revisionNumber()==openRevision&&!project.snapshot().tracks[0].clips[0].notes[1].utauFlagCurveEnabled);
    const auto click=[this](juce::Rectangle<float> bounds) {
        const auto at=bounds.getCentre();
        const auto e=juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),at,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&pianoRoll,&pianoRoll,
            juce::Time::getCurrentTime(),at,juce::Time::getCurrentTime(),1,false);
        pianoRoll.mouseDown(e);pianoRoll.mouseUp(e);project.dispatchPendingMessages();pianoRoll.diagnosticRefresh();
    };
    const auto capture=[this,output](const char* name) {
        if(auto stream=output.getChildFile(name).createOutputStream())
            return juce::PNGImageFormat().writeImageToStream(pianoRoll.createComponentSnapshot(pianoRoll.flagLaneBounds().toNearestInt()),*stream);
        return false;
    };
    check("actual_switch_screenshot",capture("actual-switch.png"));
    click(pianoRoll.flagLaneLayerBounds(false));
    check("lower_offset_switch_keeps_corresponding_parameter",pianoRoll.flagLaneFlag()=="DS:BREC"
        &&project.revisionNumber()==openRevision&&curves(0)==before0&&curves(1)==before1);
    check("offset_switch_screenshot",capture("offset-switch.png"));
    click(pianoRoll.flagLaneOffsetEnableBounds());
    check("lower_enable_applies_only_to_selection",project.snapshot().tracks[0].clips[0].notes[1].utauFlagCurveEnabled
        &&!project.snapshot().tracks[0].clips[0].notes[0].utauFlagCurveEnabled&&curves(1)==before1);
    click(pianoRoll.flagLaneOffsetEnableBounds());
    check("lower_disable_preserves_curves",!project.snapshot().tracks[0].clips[0].notes[1].utauFlagCurveEnabled&&curves(1)==before1);
    const auto switchRevision=project.revisionNumber();
    click(pianoRoll.flagLaneLayerBounds(true));
    check("lower_actual_switch_preserves_data",pianoRoll.flagLaneFlag()=="DS:ABS:BREC"&&project.revisionNumber()==switchRevision);
    pianoRoll.setFlagLaneFlag("DS:DYN");click(pianoRoll.flagLaneLayerBounds(true));
    check("DYN_returns_to_last_actual_parameter",pianoRoll.flagLaneFlag()=="DS:ABS:BREC");
    selectFlagEditMode(10);check("actual_menu_opens_lane",pianoRoll.flagLaneFlag().startsWith("DS:ABS:")&&!diffSingerBusy);
    selectFlagEditMode(11);check("offset_menu_opens_flag_lane",pianoRoll.flagLaneFlag()=="DS:BREC");
    for(int width:{900,1400}) {
        setSize(width,860);
        check("layer_controls_do_not_overlap",!pianoRoll.flagLaneLayerBounds(false).intersects(pianoRoll.flagLaneSwitchBounds())
            &&!pianoRoll.flagLaneLayerBounds(true).intersects(pianoRoll.flagLaneOffsetEnableBounds())
            &&!pianoRoll.flagLaneZoomButtonBounds(true).intersects(pianoRoll.flagLaneOffsetEnableBounds()));
    }
    flagCurveButton.onClick();check("FLAG_returns_from_offset_to_actual",pianoRoll.flagLaneFlag()=="DS:ABS:BREC");
    const auto revision=project.revisionNumber();selectPitchPointMode(13);
    check("pitch_menu_cannot_clear_actual_parameters",project.revisionNumber()==revision);
    auto menu=flagEditModeMenu();bool local=false,full=false;
    for(juce::PopupMenu::MenuItemIterator it(menu);it.next();) {
        if(it.getItem().itemID==12)local=it.getItem().isEnabled;
        if(it.getItem().itemID==14)full=it.getItem().isEnabled;
    }
    check("parameter_menu_offers_local_and_full",local&&full);
    const auto noteMenuIds=pianoRoll.diagnosticEnabledNoteMenuIds("n1");
    check("note_menu_offers_local_FLAG",std::find(noteMenuIds.begin(),noteMenuIds.end(),27)!=noteMenuIds.end());
    // Right-clicking a different note must target that note, even with stale track focus.
    pianoRoll.setSelectedNoteIds({"n0"});selectedTrackId.clear();
    pianoRoll.applyNoteMenuChoice("n1",27,0,{});
    check("local_note_menu_starts_prediction",diffSingerBusy);
    wait([this,check,finish,wait,curves,before0,before1,output] {
        project.dispatchPendingMessages();
        check("local_menu_updates_selected_note",curves(1)!=before1);
        check("local_menu_preserves_other_note_exactly",curves(0)==before0);
        const auto n=project.snapshot().tracks[0].clips[0].notes[1];
        check("local_menu_keeps_FLAG_offset",flagCurvePointsFor(n,"DS:BREC").size()==2&&flagCurvePointsFor(n,"DS:BREC")[0].value==50);
        check("local_menu_stores_dense_prediction",flagCurvePointsFor(n,"DS:REF:BREC").size()>20);
        const auto after1=curves(1);project.undo();project.dispatchPendingMessages();
        check("local_retake_single_undo",curves(0)==before0&&curves(1)==before1);
        project.redo();project.dispatchPendingMessages();check("local_retake_redo_exact",curves(1)==after1);
        juce::String error;check("save_after_local_retake",project.save(output.getChildFile("retake.hjpx"),error));
        pianoRoll.setSelectedNoteIds({"n1"});selectFlagEditMode(14);
        check("full_menu_starts_with_selection_present",diffSingerBusy);
        wait([this,check,finish,curves,before0] {
            project.dispatchPendingMessages();check("full_menu_updates_unselected_too",curves(0)!=before0);
            const auto saved0=curves(0);pianoRoll.setSelectedNoteIds({"n1"});selectFlagEditMode(13);project.dispatchPendingMessages();
            const auto n=project.snapshot().tracks[0].clips[0].notes[1];
            check("clear_menu_releases_selected_only",flagCurvePointsFor(n,"DS:ABS:BREC").empty()&&curves(0)==saved0);
            check("clear_menu_keeps_FLAG_offset",flagCurvePointsFor(n,"DS:BREC").size()==2);
            finish();
        });
    });
}
}
