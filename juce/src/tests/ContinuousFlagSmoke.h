#pragma once
#include "../MainComponent.h"
#include "../FlagCurveDrawing.h"
#include <iostream>

namespace hachi
{
inline bool MainComponent::diagnosticContinuousFlag(const juce::File& output)
{
    output.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const juce::String& name, bool passed) {
        auto* item = new juce::DynamicObject(); item->setProperty("name", name); item->setProperty("passed", passed);
        checks.add(item); ok = ok && passed;
        std::cout << name << '=' << passed << std::endl;
    };
    const auto eventFor = [](juce::Component& c, juce::Point<float> at, int mods) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at,
            juce::ModifierKeys(mods), 0,0,0,0,0, &c,&c,juce::Time::getCurrentTime(),at,
            juce::Time::getCurrentTime(),1,false);
    };
    check("existing_point_mode_toolbar", diagnosticClassicFlagLane({}));
    const auto originalMode = pianoRoll.flagEditMode();
    const auto originalPref = preferences->getValue("ui.continuousFlag");
    const auto hadPref = preferences->containsKey("ui.continuousFlag");
    const auto toolbarRevision = project.revisionNumber();
    selectFlagEditMode(2);
    pianoRoll.setFlagEditMode(PianoRollComponent::FlagEditMode::perNote);
    applyPreferences();
    check("mode_preference_roundtrip", pianoRoll.flagEditMode() == PianoRollComponent::FlagEditMode::continuous);
    auto menu = flagEditModeMenu();
    bool checked = false;
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().itemID == 2) checked = it.getItem().isTicked;
    check("right_click_menu_checked", checked);
    for (auto* button : {&flagCurveButton, &flagEnvelopeButton})
    {
        const auto toggled = button->getToggleState();
        button->mouseDown(eventFor(*button,{5,5},juce::ModifierKeys::rightButtonModifier));
        static_cast<juce::Component*>(button)->mouseUp(eventFor(*button,{5,5},juce::ModifierKeys::rightButtonModifier));
        check("right_click_does_not_toggle", button->getToggleState() == toggled
            && project.revisionNumber() == toolbarRevision);
        juce::PopupMenu::dismissAllActiveMenus();
    }
    selectFlagEditMode(1);
    check("switch_back_to_points", pianoRoll.flagEditMode() == PianoRollComponent::FlagEditMode::perNote);
    if (hadPref) preferences->setValue("ui.continuousFlag", originalPref);
    else preferences->removeValue("ui.continuousFlag");
    preferences->saveIfNeeded(); pianoRoll.setFlagEditMode(originalMode);

    for (const bool ds : {false, true})
    {
        const auto prefix = juce::String(ds ? "DS_" : "WCSNDM_");
        ProjectModel model;
        ProjectData data;
        TrackData track; track.id="track"; track.name="Continuous FLAG";
        track.compose = true; track.pitchAlgorithm=PitchAlgorithm::utau;
        if (ds)
        {
            const auto bank=output.getChildFile("fixture-bank");bank.createDirectory();
            bank.getChildFile("dsconfig.yaml").replaceWithText("acoustic: fixture.onnx\n");
            track.voicebankDirectory=bank;
        }
        ClipData clip;clip.id="clip";clip.durationSeconds=5;
        for(int i=0;i<4;++i)
        {
            NoteData note;note.id="n"+juce::String(i);note.label="a";
            note.startSeconds=.5+i;note.durationSeconds=1;note.midiNote=60;
            note.utauFlagCurveEnabled=i!=2;clip.notes.push_back(note);
        }
        track.clips.push_back(clip);data.tracks.push_back(track);
        auto other=track;other.id="other";other.clips[0].id="other-clip";
        for(auto& n:other.clips[0].notes)n.id="other-"+n.id;
        data.tracks.push_back(other);model.replace(data);
        const juce::String flag=ds ? "DS:DYN" : "g", untouched=ds ? "DS:GENC" : "Mb";
        const float low=ds ? -80.f : -30.f, high=ds ? -20.f : 30.f;
        model.setNoteUtauFlagCurve("n0",flag,{{0,low},{1,high,PitchCurveShape::smooth}});
        model.setNoteUtauFlagCurve("n1",untouched,{{0,12},{1,20}});
        const auto noteIn = [](const ProjectData& d, const juce::String& id) {
            for(const auto& t:d.tracks)for(const auto& c:t.clips)for(const auto& n:c.notes)if(n.id==id)return n;
            return NoteData{};
        };
        const auto curve = [&](const juce::String& id) {return flagCurvePointsFor(noteIn(model.snapshot(),id),flag);};
        model.dispatchPendingMessages();
        I18n labels;PianoRollComponent roll(model,labels);
        roll.setSize(1100,900);roll.setFocusedTrack("track");roll.setFocusedClip("clip");
        roll.diagnosticRefresh();roll.setSelectedNoteIds({"n0"});
        roll.setDiffSingerFlagContext(ds,juce::JSON::parse(R"([{"key":"DS:DYN","supported":true},{"key":"DS:GENC","supported":true},{"key":"DS:BREC","supported":false}])"));
        roll.setFlagLaneFlag(flag);roll.setTool(PianoRollComponent::Tool::flagCurve);
        roll.setFlagEditMode(PianoRollComponent::FlagEditMode::continuous);
        const auto at = [&](double t,float value) {return juce::Point<float>(roll.diagnosticEdgeX(t),roll.flagLaneY(value));};
        const auto mouse = [&](int kind,double t,float value) {
            const auto e=eventFor(roll,at(t,value),juce::ModifierKeys::leftButtonModifier);
            if(kind==0)roll.mouseDown(e);else if(kind==1)roll.mouseDrag(e);else roll.mouseUp(e);
        };
        const auto before=model.snapshot();const auto revision=model.revisionNumber();
        const auto keyBefore=AudioEngine::diagnosticUtauRenderKey(before,"clip");
        mouse(0,1.0,low);mouse(1,2.0,high);
        check(prefix+"preview_no_project_write",model.revisionNumber()==revision);
        mouse(2,2.0,high);
        check(prefix+"one_atomic_revision",model.revisionNumber()==revision+1);
        check(prefix+"boundary_same_value",std::abs(flagCurveValueAt(curve("n0"),1)-flagCurveValueAt(curve("n1"),0))<.01f);
        check(prefix+"crosses_unselected_note",!curve("n1").empty() && std::abs(flagCurveValueAt(curve("n1"),.3)-(low+(high-low)*.8f))<.05f);
        bool outside=true;
        const auto base=flagCurvePointsFor(noteIn(before,"n0"),flag);
        for(double t=0;t<.499;t+=.001)outside=outside&&std::abs(flagCurveValueAt(curve("n0"),t)-flagCurveValueAt(base,t))<.015f;
        check(prefix+"untouched_bezier_preserved",outside);
        check(prefix+"other_parameter_preserved",flagCurvePointsFor(noteIn(model.snapshot(),"n1"),untouched).size()==2);
        check(prefix+"other_track_and_disabled_preserved",curve("other-n0").empty()&&curve("n2").empty());
        check(prefix+"render_cache_invalidated",AudioEngine::diagnosticUtauRenderKey(model.snapshot(),"clip")!=keyBefore);
        const auto edited=model.snapshot();model.undo();
        check(prefix+"single_undo_all_notes",curve("n0").size()==base.size()&&curve("n1").empty());
        model.redo();check(prefix+"redo_all_notes",!curve("n1").empty());
        juce::String error;const auto file=output.getChildFile(prefix+"curves.hjpx");
        ProjectModel loaded;
        check(prefix+"save_load",model.save(file,error)&&loaded.load(file,error)
            &&flagCurvePointsFor(noteIn(loaded.snapshot(),"n1"),flag).size()==curve("n1").size());
        backend::UtauRenderRequest request;request.voicebankDirectory=track.voicebankDirectory;
        request.notes=AudioEngine::diagnosticUtauRequestNotes(model.snapshot(),"clip");
        if(ds)
        {
            const auto json=backend::DiffSingerRenderer::requestJson(request,"render");
            check(prefix+"render_receives_both_curves",json["notes"][0]["expressions"][juce::Identifier(flag)].size()>1&&json["notes"][1]["expressions"][juce::Identifier(flag)].size()>1);
            roll.setFlagLaneFlag("DS:BREC");check(prefix+"unsupported_parameter_blocked",roll.flagLaneFlag()==flag);
        }
        else check(prefix+"render_receives_both_curves",request.notes.size()==4&&!request.notes[0].flagCurves.empty()&&!request.notes[1].flagCurves.empty());
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        auto rev=model.revisionNumber();mouse(0,1.0,high);mouse(1,2.0,low);
        roll.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));mouse(2,2.0,low);
        check(prefix+"escape_cancels",model.revisionNumber()==rev);
        // Start in a gap, sweep backwards over a disabled note and into enabled notes.
        mouse(0,4.8,high);mouse(1,1.0,low);mouse(2,1.0,low);
        check(prefix+"backwards_gap_disabled",!curve("n3").empty()&&curve("n2").empty());
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        mouse(0,1.0,low);mouse(1,2.0,high);mouse(1,1.7,low);mouse(2,1.7,low);
        check(prefix+"retracing_last_pass",std::abs(flagCurveValueAt(curve("n1"),.2)-low)<.05f);
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        mouse(0,1.0,low);model.setNoteLabel("n0","b");rev=model.revisionNumber();mouse(2,2.0,high);
        check(prefix+"stale_preview_rejected",model.revisionNumber()==rev&&noteIn(model.snapshot(),"n0").label=="b");
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        if(ds)
        {
            auto waves=std::make_shared<std::vector<UtauNoteWaveform>>();
            const auto waveData=model.snapshot();
            for(const auto& n:waveData.tracks.front().clips.front().notes)
            {
                UtauNoteWaveform wave;wave.noteId=n.id;wave.audioHash=AudioEngine::utauNoteAudioHash(n);
                wave.phonemes={{"zh/b","C",-.15,0},{"zh/ang","V",0,1}};waves->push_back(wave);
            }
            roll.setUtauNoteWaveforms(waves);roll.diagnosticRefresh();
            std::cout << "DS_span_debug bank=" << waveData.tracks.front().voicebankDirectory.getFullPathName()
                << " dir=" << waveData.tracks.front().voicebankDirectory.isDirectory() << " ds=" << trackIsDiffSinger(waveData.tracks.front())
                << " waves=" << waves->size() << " span=" << roll.flagLaneSpanFor(noteIn(model.snapshot(),"n1")).first << std::endl;
            for(const auto& w:*waves)
                std::cout << "DS_wave_debug " << w.noteId << " hash=" << w.audioHash
                    << " current=" << AudioEngine::utauNoteAudioHash(noteIn(model.snapshot(),w.noteId)) << std::endl;

            check(prefix+"negative_span_available",roll.flagLaneSpanFor(noteIn(model.snapshot(),"n1")).first<-.1);
            mouse(0,1.4,low);mouse(1,1.7,high);mouse(2,1.7,high);
            check(prefix+"negative_consonant_time",std::abs(flagCurveValueAt(curve("n1"),-.1)-low)<.05f);
            check(prefix+"overlapping_consonants_share_value",std::abs(flagCurveValueAt(curve("n1"),-.05)-flagCurveValueAt(curve("n0"),.95))<.05f);
        }
        else
        {
            roll.setFlagLaneFlag("b");
            mouse(0,1.48,20);mouse(1,1.8,80);mouse(2,1.8,80);
            const auto onset=flagCurvePointsFor(noteIn(model.snapshot(),"n1"),"b");
            check(prefix+"onset_negative_offset",!onset.empty()&&onset.front().timeSeconds<0);
            check(prefix+"onset_does_not_edit_vowel",std::abs(flagCurveValueAt(onset,.2))<.01f);
            roll.setFlagLaneFlag(flag);
        }
        const auto laterClip=model.addClip("track",5,2);
        const auto laterNote=model.addNote(laterClip,5.5,1,62);
        model.setNotesUtauFlagCurveEnabled({laterNote},true);
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        mouse(0,4.3,low);mouse(1,6,high);mouse(2,6,high);
        check(prefix+"focused_region_blocks_other_clip",curve(laterNote).empty());
        // A track overview still supports a stroke across its visible regions.
        roll.setFocusedClip({});
        mouse(0,4.3,low);mouse(1,6,high);mouse(2,6,high);
        check(prefix+"cross_clip_time_offset",!curve(laterNote).empty()
            &&std::abs(flagCurveValueAt(curve(laterNote),.3)-(low+(high-low)*1.5f/1.7f))<.05f);
        model.dispatchPendingMessages();roll.diagnosticRefresh();
        const auto lane=roll.flagLaneBounds().expanded(2).toNearestInt();
        if(auto stream=output.getChildFile(prefix+"continuous.png").createOutputStream())
            juce::PNGImageFormat().writeImageToStream(roll.createComponentSnapshot(lane),*stream);
    }
    auto* result=new juce::DynamicObject();result->setProperty("passed",ok);result->setProperty("checks",checks);
    output.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(result),true));
    return ok;
}
}

