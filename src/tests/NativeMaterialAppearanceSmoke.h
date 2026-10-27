#pragma once
#include "../NativeSourceReplacementPlan.h"
#include "../NativeMaterialContinuity.h"
#include "../NativeSourceEdgePlan.h"
#include "../PianoRollComponent.h"
#include "../SettingsComponent.h"
#include "../OttoMaterialIO.h"
#include "../Theme.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#ifdef near
#undef near
#endif
#ifdef far
#undef far
#endif
#ifdef small
#undef small
#endif
#endif
namespace hachi
{
inline bool runNativeMaterialAppearanceSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("material-appearance-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+(error.isEmpty()?juce::String{}:" | "+error)+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-7;};
    const auto pump=[&](int milliseconds)
    {
        const auto until=juce::Time::getMillisecondCounterHiRes()+milliseconds;
        do{
#if JUCE_WINDOWS
            MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message==WM_QUIT){PostQuitMessage(int(msg.wParam));break;}TranslateMessage(&msg);DispatchMessageW(&msg);}
#endif
            juce::Thread::sleep(2);
        }while(juce::Time::getMillisecondCounterHiRes()<until);
    };
    const auto wav=root.getChildFile("registered.wav");
    {juce::AudioBuffer<float> pcm(1,96000);for(int i=0;i<96000;++i){const auto time=i/48000.0;const auto gain=.5+.28*std::sin(time*13);pcm.setSample(0,i,float(gain*std::sin(time*juce::MathConstants<double>::twoPi*220)));}auto stream=wav.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,24,{},0));check("physical_control_wave",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));}
    SampleRegionSetting a;a.materialAudioId="voice";a.materialRegionId="da";a.name="da";a.regionEndSeconds=.6;a.alignmentSeconds=.1;a.fixedDurationSeconds=.16;a.role=NativeSegmentRole::vowel;a.nativeUnpitched=false;a.connectedNextRegionId="re";
    SampleRegionSetting b=a;b.materialRegionId="re";b.name="re";b.regionStartSeconds=.6;b.regionEndSeconds=1.2;b.alignmentSeconds=.75;b.connectedNextRegionId.clear();
    // Regression: one pronunciation carried by C + two V zones must not
    // produce one copy of its name over every zone/P side.
    for(auto* row:{&a,&b})
    {
        row->vowelSplitsExplicit=true;
        const double cuts[]{row->regionStartSeconds,row->regionStartSeconds+row->fixedDurationSeconds,row->regionStartSeconds+.35,row->regionEndSeconds};
        for(int i=0;i<3;++i){NativeSegment s;s.id=row->materialRegionId+"-"+juce::String(i);s.alias=row->name;s.role=i?NativeSegmentRole::vowel:NativeSegmentRole::consonant;s.sourceStartSeconds=cuts[i];s.sourceEndSeconds=cuts[i+1];s.alignmentSeconds=row->alignmentSeconds;s.stretchable=i>0;s.stretchWeight=1;row->segments.push_back(s);}
    }
    auto edges=[](SampleRegionSetting& row){NativePronunciationEdges e;e.entryFromSourceSeconds=row.regionStartSeconds;e.entryToSourceSeconds=row.regionStartSeconds+.015;e.exitFromSourceSeconds=row.regionEndSeconds-.025;e.exitToSourceSeconds=e.terminationSourceSeconds=row.regionEndSeconds;e.provenance="registered-control";row.nativePronunciationEdges=e;};edges(a);edges(b);
    auto rows=std::make_shared<const std::vector<SampleRegionSetting>>(std::vector<SampleRegionSetting>{a,b});OttoMaterialIO::Document doc;doc.audio={{"voice",wav.getFileName(),*rows}};check("registered_complete_variant",OttoMaterialIO::saveDocument(root.getChildFile("otto.ini"),doc,error));
    NativeScoreMaterialSource na;na.noteId="A";na.audio=wav;na.audioId="voice";na.regionId="da";na.row=a;na.completeRows=rows;na.scoreStart=.3;na.scoreDuration=1;na.preutteranceKnown=true;na.targetPreutterance=.1;
    auto nb=na;nb.noteId="B";nb.regionId="re";nb.row=b;nb.scoreStart=1.3;nb.scoreDuration=.9;nb.targetPreutterance=.15;
    std::vector<NativeScoreMaterialGroup> plan;check("one_common_source_clock",planNativeScoreMaterialClocks({na,nb},plan,error)&&plan.size()==1);
    if(plan.empty())return false;const auto cut=nativeTargetTimeAt(plan[0].clock,.6);
    check("registered_cut_before_P_inside_first_score_note",near(cut,1.15)&&cut>na.scoreStart&&cut<na.scoreStart+na.scoreDuration&&near(nativeTargetTimeAt(plan[0].clock,.75),1.3));
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;ClipData score;score.id="score";score.sourceFile=root.getChildFile("score.ust");score.durationSeconds=score.sourceDurationSeconds=2.2;
    for(int i=0;i<2;++i){NoteData n;n.id=i?"B":"A";n.label=i?"re":"da";n.startSeconds=i?1.3:.3;n.durationSeconds=i?.9:1;n.midiNote=i?64:60;n.utauPreutteranceOverrideEnabled=true;n.utauPreutteranceSeconds=i?.15:.1;score.notes.push_back(n);}track.clips={score};seed.tracks={track};ProjectModel model;model.resetDocument(seed);
    const auto importedView=nativeNoteVisualSpan(seed.tracks[0].clips[0].notes[0],seed.tracks[0].clips[0]);
    check("imported_score_body_is_post_P_and_prefix_extends_before_it",near(importedView.p,.3)&&near(importedView.from,.2)&&near(importedView.scoreFrom,.3)&&near(nativeNotePreutteranceTarget(seed.tracks[0].clips[0].notes[0],seed.tracks[0].clips[0]),.3));
    ProjectModel protocolAlignment;protocolAlignment.resetDocument(seed);
    check("protocol_double_click_alignment_targets_actual_P_not_P_plus_lead",protocolAlignment.alignNotePreutteranceToGrid("A",.25,error)&&near(protocolAlignment.snapshot().tracks[0].clips[0].notes[0].startSeconds,.25));
    const auto assigned=model.replaceNotesMaterialSources({{"A",wav,rows,"voice","da","da"},{"B",wav,rows,"voice","re","re"}},model.revisionNumber(),error);check("same_source_assignment_atomic",!assigned.rejected&&assigned.changed);
    // Exact controlled 220Hz source facts, not a claimed detector/NN result.
    NoteData measured;measured.startSeconds=0;measured.durationSeconds=2;measured.midiNote=measured.sourceMidiCenter=57;measured.sourcePitchMeasured=true;measured.contour={{0,0,0,true},{2,0,0,true}};
    for(const auto& ticket:assigned.tickets)check("controlled_source_facts_for_view_only",model.completeMaterialSourceAnalysis(ticket,{measured},error));
    const auto data=model.snapshot();std::vector<NativeMaterialSeam> seams;check("actual_placed_source_boundary_not_pitch_graph",nativeMaterialSeams(data.tracks[0],seams,error)&&seams.size()==1&&near(seams[0].sourceSeconds,.6)&&near(seams[0].targetSeconds,1.15)&&data.nativeConnections.empty());
    NativeSourceEdgePlan edgePlan;check("audio_edge_ownership_uses_same_projected_cut",resolveNativeSourceEdgePlan(data.tracks[0],{"A","B"},edgePlan,error)&&edgePlan.members.size()==2&&near(edgePlan.members[0].supportToTargetSeconds,1.15)&&near(edgePlan.members[1].supportFromTargetSeconds,1.15));
    auto wrong=data.tracks[0];wrong.clips[0].notes[1].materialAudioId="other";std::vector<NativeMaterialSeam> noProof;check("different_variant_never_fake_same_source_seam",nativeMaterialSeams(wrong,noProof,error)&&noProof.empty());
    auto transition=b;transition.pronunciationKind=PronunciationKind::transition;auto nt=nb;nt.row=transition;std::vector<NativeScoreMaterialGroup> transitionPlan;
    check("registered_transition_has_no_P_even_with_old_target_override",near(pronunciationTimingFor(transition).alignmentSeconds,.6)&&planNativeScoreMaterialClocks({na,nt},transitionPlan,error)&&near(nativeTargetTimeAt(transitionPlan[0].clock,.6),1.3));
    auto transitionEdit=transition;check("transition_registration_setter_cannot_reintroduce_lead",editPronunciationAlignment(transitionEdit,.85,error)&&near(transitionEdit.alignmentSeconds,.6));
    I18n strings;strings.setLanguage(I18n::Language::enUS);PianoRollComponent roll(model,strings);roll.setReadsVoicebankInBackground(false);roll.diagnosticSetCulling(false);roll.setFocusedTrack("native");roll.setPixelsPerSecond(280);roll.setRowHeight(28);roll.setSize(1050,3600);roll.setShowEnvelope(false);roll.setShowPitchLine(false);roll.setShowOriginalPitchLine(false);roll.setShowNoteRange(true);roll.setShowNativeWaveforms(true);roll.diagnosticRefresh();pump(200);
    check("view_keeps_cut_after_child_expansion",roll.diagnosticMaterialSeams().size()==1&&near(roll.diagnosticMaterialSeams()[0].targetSeconds,1.15));
    const int cropY=int(roll.diagnosticYForMidi(66))-20;
    const auto capture=[&](const juce::String& name)
    {
        juce::Image image(juce::Image::ARGB,1050,300,true,juce::SoftwareImageType{});juce::Graphics g(image);g.addTransform(juce::AffineTransform::translation(0.f,float(-cropY)));roll.paintEntireComponent(g,true);juce::PNGImageFormat png;if(auto out=root.getChildFile(name+".png").createOutputStream())png.writeImageToStream(image,*out);return image;
    };
    const auto brightness=[&](const juce::Image& image,double time,float midi){double value=0;const int x=int(roll.pixelForSeconds(time)),y=int(roll.diagnosticYForMidi(midi))-cropY;for(int dx=2;dx<9;++dx)for(int dy=-3;dy<4;++dy)value+=image.getPixelAt(x+dx,y+dy).getPerceivedBrightness();return value/49;};
    for(const auto& theme:{juce::String("dark"),juce::String("light")})for(auto style:{PianoRollComponent::NoteStyle::mld,PianoRollComponent::NoteStyle::rep})
    {
        Palette::applyTheme(theme,juce::Colour(0xff7f69ca),juce::Colour(0xffcbcbfa),juce::Colour(0xfff4c000));
        const auto name=theme+"-"+(style==PianoRollComponent::NoteStyle::mld?juce::String("mld"):juce::String("rep"));roll.setNoteStyle(style);roll.clearNoteSelection();auto dark=capture(name+"-unselected");roll.setSelectedNoteIds({"A"});auto bright=capture(name+"-selected-A");
        check((name+"_selection_increases_only_selected_brightness").toRawUTF8(),brightness(bright,.7,60)>brightness(dark,.7,60)+.04&&std::abs(brightness(bright,1.7,64)-brightness(dark,1.7,64))<.002);
        roll.setSelectedNoteIds({"A","B"});const auto multi=capture(name+"-multi");
        check((name+"_multi_brightens_each_selected_note").toRawUTF8(),brightness(multi,.7,60)>brightness(dark,.7,60)+.04&&brightness(multi,1.7,64)>brightness(dark,1.7,64)+.04);
        roll.clearNoteSelection();const auto cleared=capture(name+"-clear");
        check((name+"_cancel_restores_imported_dark_base").toRawUTF8(),std::abs(brightness(cleared,.7,60)-brightness(dark,.7,60))<.002&&std::abs(brightness(cleared,1.7,64)-brightness(dark,1.7,64))<.002);
    }
    roll.setTool(PianoRollComponent::Tool::note);roll.clearNoteSelection();
    {const juce::Point<float> point(float(roll.pixelForSeconds(1.2)),roll.diagnosticYForMidi(64));const auto now=juce::Time::getCurrentTime();
     const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,&roll,&roll,now,point,now,1,false);roll.mouseDown(event);roll.mouseUp(event);}
    check("visible_preutterance_hit_selects_its_actual_note",roll.selectedNoteIds()==std::vector<juce::String>{"B"});
    roll.setSelectedNoteIds({"A"});
    check("move_connected_group_keeps_registered_cut_clock",model.moveNativeNotes({"A","B"},.2,0,&error));roll.diagnosticRefresh();
    check("view_cut_tracks_group_movement",roll.diagnosticMaterialSeams().size()==1&&near(roll.diagnosticMaterialSeams()[0].targetSeconds,1.35)&&roll.selectedNoteIds()==std::vector<juce::String>{"A"});capture("moved-selected-A");
    Palette::applyTheme("dark",juce::Colour(0xff7f69ca),juce::Colour(0xffcbcbfa),juce::Colour(0xfff4c000));
    auto transitionRows=std::make_shared<const std::vector<SampleRegionSetting>>(std::vector<SampleRegionSetting>{a,transition});
    model.resetDocument(seed);const auto transitionAssign=model.replaceNotesMaterialSources({{"A",wav,transitionRows,"voice","da","da"},{"B",wav,transitionRows,"voice","re","re"}},model.revisionNumber(),error);
    for(const auto& ticket:transitionAssign.tickets)model.completeMaterialSourceAnalysis(ticket,{measured},error);
    roll.diagnosticRefresh();roll.setNoteStyle(PianoRollComponent::NoteStyle::rep);roll.clearNoteSelection();
    const auto transitionExpanded=expandedClipParts(model.snapshot().tracks[0].clips[0],true);
    bool greenWithoutLead=false;for(const auto& child:transitionExpanded)for(const auto& note:child.notes)if(note.id=="B"){const auto span=nativeNoteVisualSpan(note,child);greenWithoutLead=span.transition&&near(span.from,span.p)&&near(span.p,1.3);}
    check("placed_registered_transition_entirely_green_no_lead",!transitionAssign.rejected&&greenWithoutLead);capture("rep-transition-no-preutterance");
    check("specified_phase_palette_not_segment_role_guess",nativeNotePhaseColour(true,false,false)==juce::Colour(0xfff4c000)&&nativeNotePhaseColour(false,false,false)==juce::Colour(0xff7f69ca)&&nativeNotePhaseColour(true,true,false)==nativeNotePhaseColour(false,true,false));
    juce::PropertiesFile::Options propertiesOptions;propertiesOptions.applicationName="appearance-test";propertiesOptions.filenameSuffix="settings";juce::PropertiesFile properties(root.getChildFile("settings.xml"),propertiesOptions);properties.setValue("ui.noteStyle","invalid");juce::AudioDeviceManager devices;
    HachiLookAndFeel settingsLook;
    SettingsComponent settings(strings,devices,properties,[&]{roll.setNoteStyle(properties.getValue("ui.noteStyle")=="rep"?PianoRollComponent::NoteStyle::rep:PianoRollComponent::NoteStyle::mld);});settings.setLookAndFeel(&settingsLook);settings.setSize(920,650);settings.selectPage(0);
    const auto find=[&](auto&& self,juce::Component& c,const juce::String& id)->juce::Component*{if(c.getComponentID()==id)return &c;for(int i=0;i<c.getNumChildComponents();++i)if(auto* found=self(self,*c.getChildComponent(i),id))return found;return nullptr;};
    auto* choice=dynamic_cast<juce::ComboBox*>(find(find,settings,"settings-note-style"));auto* apply=dynamic_cast<juce::Button*>(find(find,settings,"settings.apply"));check("real_interface_style_control",choice&&choice->getNumItems()==2&&choice->getItemText(0)=="mld"&&choice->getItemText(1)=="rep");
    check("invalid_style_defaults_to_mld",choice&&choice->getSelectedId()==1);
    if(choice&&apply){choice->setSelectedId(2);apply->triggerClick();pump(40);check("settings_rep_persists_and_applies",properties.getValue("ui.noteStyle")=="rep"&&roll.noteStyleValue()==PianoRollComponent::NoteStyle::rep);}else check("settings_apply_button",false);
    const auto settingsShot=[&](const juce::String& name){juce::Image image(juce::Image::ARGB,920,650,true,juce::SoftwareImageType{});juce::Graphics graphics(image);settings.paintEntireComponent(graphics,true);juce::PNGImageFormat png;if(auto out=root.getChildFile(name+".png").createOutputStream())png.writeImageToStream(image,*out);return image;};
    auto* themeChoice=dynamic_cast<juce::ComboBox*>(find(find,settings,"settings-theme"));
    if(choice&&apply&&themeChoice)
    {
        // Emulate another app entry selecting light while this settings window
        // still holds an old dark combo. A style apply must not overwrite it.
        properties.setValue("ui.theme","light");Palette::applyTheme("light",juce::Colour(0xff7f69ca),juce::Colour(0xffcbcbfa),juce::Colour(0xfff4c000));settingsLook.refreshColours();
        choice->setSelectedId(1,juce::sendNotificationSync);apply->triggerClick();pump(40);
        auto light=settingsShot("settings-style-preserves-light");
        check("note_style_only_apply_never_resets_current_light_theme",properties.getValue("ui.theme")=="light"&&themeChoice->getSelectedId()==2&&Palette::panel.getPerceivedBrightness()>.8f);
        check("open_settings_repaints_its_own_light_page_immediately",light.getPixelAt(450,500)==Palette::panel);
        bool editorInk=true;int nonemptyEditors=0;const auto inspectEditors=[&](auto&& self,juce::Component& component)->void{if(auto* editor=dynamic_cast<juce::TextEditor*>(&component)){if(editor->getText().isNotEmpty()){++nonemptyEditors;editorInk&=editor->findColour(juce::TextEditor::textColourId)==Palette::text;}}for(int i=0;i<component.getNumChildComponents();++i)self(self,*component.getChildComponent(i));};inspectEditors(inspectEditors,settings);
        check("settings_existing_text_stays_readable_and_values_not_cleared",editorInk&&nonemptyEditors>=3&&properties.getValue("ui.accent")=="7F69CA"&&properties.getValue("ui.accentLight")=="CBCBFA");
        themeChoice->setSelectedId(1,juce::sendNotificationSync);apply->triggerClick();pump(40);auto dark=settingsShot("settings-live-dark");
        check("explicit_dark_theme_refreshes_same_settings_window",properties.getValue("ui.theme")=="dark"&&dark.getPixelAt(450,500)==Palette::panel&&Palette::panel.getPerceivedBrightness()<.3f);
        themeChoice->setSelectedId(2,juce::sendNotificationSync);apply->triggerClick();pump(40);light=settingsShot("settings-live-light");
        check("explicit_light_theme_refreshes_same_settings_window",properties.getValue("ui.theme")=="light"&&light.getPixelAt(450,500)==Palette::panel&&Palette::panel.getPerceivedBrightness()>.8f);
        choice->setSelectedId(2,juce::sendNotificationSync);apply->triggerClick();pump(40);
        check("switching_rep_after_theme_edit_keeps_light",properties.getValue("ui.theme")=="light"&&themeChoice->getSelectedId()==2&&roll.noteStyleValue()==PianoRollComponent::NoteStyle::rep);
    }
    else check("theme_regression_controls_available",false);
    juce::Image settingsImage(juce::Image::ARGB,920,650,true,juce::SoftwareImageType{});{juce::Graphics graphics(settingsImage);settings.paintEntireComponent(graphics,true);}juce::PNGImageFormat png;if(auto out=root.getChildFile("interface-note-style.png").createOutputStream())png.writeImageToStream(settingsImage,*out);
    settings.setLookAndFeel(nullptr);
    std::cout<<"native_material_appearance_fixture="<<root.getFullPathName()<<std::endl;root.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
