#pragma once
#include "../AdvancedEnvelopePanel.h"
namespace hachi
{
inline bool MainComponent::diagnosticAdvancedEnvelopePanel(const juce::File& folder)
{
    folder.createDirectory();bool ok=diagnosticAdvancedEnvelope(folder.getChildFile("baseline"));stopTimer();
    const auto check=[&](const char* name,bool v){ok=ok&&v;std::cout<<name<<'='<<v<<std::endl;};
    const auto near=[](double a,double b){return std::abs(a-b)<4.e-4;};
    const backend::TailFadeSettings settings{.2,.8,.8,.15,2.0};
    check("settings_validate",settings.valid());
    auto invalid=settings;invalid.endFraction=.1;check("reversed_range_rejected",!invalid.valid());invalid=settings;invalid.startGain=std::numeric_limits<double>::infinity();check("nonfinite_level_rejected",!invalid.valid());invalid=settings;invalid.endGain=1.2;check("rising_gain_rejected",!invalid.valid());
    check("gain_before_range_and_after_end",near(backend::tailFadeGain(2,-1,0,1,settings),.8)&&near(backend::tailFadeGain(2,1,0,1,settings),.15));
    check("linear_stays_straight_regardless_of_curve_power",near(backend::tailFadeGain(1,.5,0,1,settings),.8-(.8-.15)*.5));
    check("custom_s_curve_power_matches_value",near(backend::tailFadeGain(2,.5,0,1,settings),.8-(.8-.15)*.15625));
    check("custom_range_follows_tail_length",near(backend::tailFadeGain(1,3,2,4,settings),backend::tailFadeGain(1,.5,0,1,settings)));
    check("disabled_effect_is_unity",near(backend::tailFadeGain(0,.5,0,1,settings),1));
    backend::TailFadeSettings bezier;bezier.customCurve=true;
    bool legacyMatch=true,monotone=true;float previous=1;
    for(int i=0;i<=1000;++i){const auto t=i/1000.0;const auto gain=backend::tailFadeGain(2,t,0,1,bezier);
        legacyMatch=legacyMatch&&near(gain,backend::tailFadeGain(2,t,0,1));monotone=monotone&&gain<=previous+1.e-6f;previous=gain;}
    check("bezier_s_matches_legacy_and_monotone",legacyMatch&&monotone);
    bezier.control1Time=0;bezier.control2Time=0;bezier.control1Progress=.2;bezier.control2Progress=.8;
    check("bezier_vertical_tangents_finite",std::isfinite(backend::tailFadeGain(2,.00001,0,1,bezier)));
    auto badBezier=bezier;badBezier.control1Time=.9;check("bezier_reversed_controls_rejected",!badBezier.valid());
    bezier.control1Time=.15;bezier.control2Time=.8;bezier.control1Progress=.7;bezier.control2Progress=.95;
    check("bezier_time_is_inverted",backend::tailFadeGain(2,.5,0,1,bezier)<.3);
    AdvancedEnvelopePanel customPanel({{"custom","Bezier",{0,1},{{0,0,true},{1,0,false}}, {}}},1,2,bezier,false,folder.getChildFile("presets-"+juce::Uuid().toString()+".json"));
    check("shape_menu_has_only_line_and_curve",customPanel.diagnosticShapeCount()==2);
    customPanel.setValues(2,settings);check("unified_curve_keeps_legacy_s_exact",customPanel.values()==settings&&customPanel.mode()==2);
    customPanel.diagnosticCurveControl(1,20);check("legacy_s_handles_become_editable_curve",customPanel.values().customCurve&&near(customPanel.values().control1Progress,.2)&&near(customPanel.values().curvePower,1));
    customPanel.diagnosticSelectShape(1);customPanel.diagnosticSelectShape(2);check("curve_menu_opens_handle_editor",customPanel.mode()==2&&customPanel.values().customCurve);
    customPanel.setValues(2,bezier);
    check("custom_curve_initial_values",customPanel.values().customCurve&&near(customPanel.values().control1Time,.15));
    customPanel.diagnosticDragCurveHandle(1,5,8);check("drag_control_changes_both_coordinates",customPanel.values().control1Time>.15&&customPanel.values().control1Progress>.7);
    customPanel.diagnosticCurveControl(0,99);check("control_time_clamps_to_order",customPanel.values().valid()&&near(customPanel.values().control1Time,customPanel.values().control2Time));
    customPanel.diagnosticPreset(3);check("fast_first_preset",backend::tailFadeGain(2,.5,0,1,customPanel.values())<.3);
    customPanel.diagnosticPreset(4);check("slow_first_preset",backend::tailFadeGain(2,.5,0,1,customPanel.values())>.7);
    customPanel.diagnosticPreset(5);check("uniform_bezier_preset",near(backend::tailFadeGain(2,.3,0,1,customPanel.values()),.7));
    customPanel.diagnosticPreset(2);customPanel.setSize(740,660);check("custom_controls_fit_minimum_size",customPanel.diagnosticControlsFit());
    {auto out=folder.getChildFile("custom-bezier-controls.png").createOutputStream();juce::PNGImageFormat png;check("custom_controls_snapshot",out&&png.writeImageToStream(customPanel.createComponentSnapshot(customPanel.getLocalBounds(),true,1.5f),*out));}
    const auto presetDraft=settings;auto customDraft=presetDraft;customDraft.customCurve=true;customDraft.control1Time=.15;customDraft.control1Progress=.7;customDraft.control2Time=.8;customDraft.control2Progress=.95;
    customPanel.setValues(2,customDraft);int presetApplies=0;customPanel.onApply=[&](int,const auto&,bool){++presetApplies;return juce::String();};
    check("save_named_preset",customPanel.diagnosticSavePreset(juce::String::fromUTF8("轻柔尾音")));
    check("duplicate_and_empty_names_rejected",!customPanel.diagnosticSavePreset(juce::String::fromUTF8("轻柔尾音"))&&!customPanel.diagnosticSavePreset("   ")&&customPanel.diagnosticPresetCount()==1);
    customPanel.setValues(1,{});customPanel.diagnosticPreset(100);
    check("preset_restores_full_draft_without_applying",customPanel.mode()==2&&customPanel.values()==customDraft&&presetApplies==0);
    customPanel.diagnosticApply();check("preset_apply_is_explicit",presetApplies==1);
    customPanel.diagnosticDeletePreset();check("delete_preset_keeps_current_draft",customPanel.diagnosticPresetCount()==0&&customPanel.values()==customDraft);
    const auto libraryFile=folder.getChildFile("library-"+juce::Uuid().toString()+".json");TailFadePresetStore library(libraryFile);juce::String libraryError;
    TailFadePreset savedPreset;savedPreset.name=juce::String::fromUTF8("副歌淡出");savedPreset.shape=3;savedPreset.enabled=false;savedPreset.settings=customDraft;
    check("library_save",library.add(savedPreset,libraryError));std::vector<TailFadePreset> reopened;
    check("library_survives_new_instance",TailFadePresetStore(libraryFile).load(reopened,libraryError)&&reopened.size()==1&&reopened[0].settings==customDraft&&reopened[0].shape==3&&!reopened[0].enabled);
    AdvancedEnvelopePanel reopenedPanel({},1,1,{},false,libraryFile);reopenedPanel.diagnosticPreset(100);
    check("reopened_panel_restores_disabled_custom_shape",reopenedPanel.mode()==0&&reopenedPanel.values()==customDraft);
    savedPreset.name="Linear";savedPreset.shape=1;savedPreset.enabled=true;savedPreset.settings=settings;
    TailFadePreset legacyPreset;legacyPreset.name="Legacy S";legacyPreset.shape=2;legacyPreset.settings=settings;
    const auto legacyFile=folder.getChildFile("legacy-presets-"+juce::Uuid().toString()+".json");
    check("legacy_s_preset_fixture",TailFadePresetStore(legacyFile).add(legacyPreset,libraryError));
    AdvancedEnvelopePanel legacyPanel({},1,1,{},false,legacyFile);legacyPanel.diagnosticPreset(100);
    check("legacy_s_preset_maps_to_unified_curve",legacyPanel.diagnosticShapeCount()==2&&legacyPanel.mode()==2&&legacyPanel.values()==settings);
    check("library_second_writer_preserves_first_preset",TailFadePresetStore(libraryFile).add(savedPreset,libraryError)&&library.load(reopened,libraryError)&&reopened.size()==2);
    check("library_delete_preserves_other_preset",library.remove(savedPreset.id,libraryError)&&library.load(reopened,libraryError)&&reopened.size()==1);
    libraryFile.replaceWithText("broken json");const auto broken=libraryFile.loadFileAsString();savedPreset.name="new";
    check("corrupt_library_is_not_overwritten",!library.add(savedPreset,libraryError)&&libraryFile.loadFileAsString()==broken);
    customPanel.diagnosticSavePreset(juce::String::fromUTF8("轻柔尾音"));customPanel.setSize(740,660);
    {auto out=folder.getChildFile("custom-preset-library.png").createOutputStream();juce::PNGImageFormat png;check("preset_library_snapshot",out&&png.writeImageToStream(customPanel.createComponentSnapshot(customPanel.getLocalBounds(),true,1.5f),*out));}
    const auto data=project.snapshot();ProjectModel model;model.resetDocument(data);const auto original=model.contentFingerprint();
    check("detailed_multi_note_apply",model.setNotesTailFade({"n2","n4"},1,settings));auto changed=model.snapshot();
    check("only_target_notes_receive_parameters",changed.tracks[0].clips[0].notes[0].utauTailFade==settings&&changed.tracks[0].clips[0].notes[2].utauTailFade==settings&&changed.tracks[0].clips[0].notes[1].utauTailFade==backend::TailFadeSettings{});
    check("detailed_apply_preserves_flags_and_sparse_envelope",changed.tracks[0].clips[0].notes[0].utauFlags==data.tracks[0].clips[0].notes[0].utauFlags&&changed.tracks[0].clips[0].notes[0].amplitudeEnvelope.size()==4);
    check("all_parameters_undo_together",model.undo()&&model.contentFingerprint()==original&&!model.canUndo());model.redo();
    const auto before=model.contentFingerprint();check("invalid_settings_do_not_mutate",!model.setNotesTailFade({"n2"},1,invalid)&&before==model.contentFingerprint());
    juce::String error;const auto file=folder.getChildFile("detailed-tail.hjpx");check("detailed_project_saved",model.save(file,error));ProjectModel loaded;check("detailed_project_loaded",loaded.load(file,error));
    check("all_parameters_roundtrip",loaded.snapshot().tracks[0].clips[0].notes[0].utauTailFade==settings&&loaded.snapshot().tracks[0].clips[0].notes[0].utauTailFadeMode==1);
    model.setNotesTailFade({"n2"},0);check("disable_keeps_settings_for_reenable",model.snapshot().tracks[0].clips[0].notes[0].utauTailFade==settings);model.setNotesTailFade({"n2"},2);check("shape_switch_keeps_parameters",model.snapshot().tracks[0].clips[0].notes[0].utauTailFade==settings);
    const auto beforeBezier=model.contentFingerprint();model.setNotesTailFade({"n2"},2,bezier);
    check("bezier_saved",model.save(folder.getChildFile("bezier.hjpx"),error));ProjectModel bezierLoaded;
    check("bezier_roundtrip",bezierLoaded.load(folder.getChildFile("bezier.hjpx"),error)&&bezierLoaded.snapshot().tracks[0].clips[0].notes[0].utauTailFade==bezier);
    check("bezier_undo",model.undo()&&model.contentFingerprint()==beforeBezier);
    const auto plain=data.tracks[0].clips[0].notes[0];auto edited=plain;edited.utauTailFade=settings;
    check("detailed_settings_invalidate_mix_not_raw_cache",AudioEngine::utauNoteRenderHash(plain)!=AudioEngine::utauNoteRenderHash(edited)&&AudioEngine::utauNoteAudioHash(plain)==AudioEngine::utauNoteAudioHash(edited));
    edited=plain;edited.utauTailFade=bezier;
    check("bezier_invalidate_mix_only",AudioEngine::utauNoteRenderHash(plain)!=AudioEngine::utauNoteRenderHash(edited)&&AudioEngine::utauNoteAudioHash(plain)==AudioEngine::utauNoteAudioHash(edited));
    const std::vector<AmplitudeEnvelopePoint> curve{{0,-3,false},{.1,0,true},{.7,-3,false},{1.2,-12,false}};
    const auto picture=backend::tailFadePicture(curve,2,.4,1.2,settings);
    const auto expected=[&](double t){return backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(curve,std::min(t,.56)))*backend::tailFadeGain(2,t,.4,1.2,settings);};
    check("preview_matches_actual_level_during_fade",near(backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(picture,.85)),expected(.85)));
    check("preview_holds_single_tail_end_level",near(backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(picture,1.15)),expected(1.15)));
    check("silent_base_stays_silent",near(backend::tailFadeGain(2,.5,0,1,settings)*backend::envelopeGainFromDb(-60),0));
    // An original steep fall used to bend the final part of a supposedly straight ending.
    const std::vector<AmplitudeEnvelopePoint> steep{{0,0,true},{.8,0,false},{1,-60,false}};
    const auto straight=backend::tailFadePicture(steep,1,.4,1.0);
    const auto smooth=backend::tailFadePicture(steep,2,.4,1.0);
    bool lineOk=true,curveOk=true;
    for(double t=.4;t<1;t+=.01)
    {
        const auto u=(t-.4)/.6;
        lineOk=lineOk&&near(backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(straight,t)),1-u);
        // Stored display points clamp below -60 dB to silence, unlike the float mixer.
        curveOk=curveOk&&std::abs(backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(smooth,t))-(1-u*u*(3-2*u)))<.0011;
        curveOk=curveOk&&near(backend::tailFadeEnvelopeGain(2,t,.4,1.0,{},[](double at){return at<=.8?1.0f:0.0f;}),1-u*u*(3-2*u));
    }
    check("linear_ending_does_not_bend_at_original_fall",lineOk);
    check("smooth_ending_is_one_curve_through_original_fall",curveOk);
    check("original_envelope_unchanged_when_disabled",backend::tailFadePicture(steep,0,.4,1).size()==steep.size());
    for(int mode=1;mode<=2;++mode)
    {
        AdvancedEnvelopePanel sample({{"test","single ending",{.4,1.0},steep,{}}},1,mode,{},false);
        auto out=folder.getChildFile(mode==1?"single-straight-ending.png":"single-curved-ending.png").createOutputStream();
        juce::PNGImageFormat png;check("single_ending_snapshot",out&&png.writeImageToStream(sample.createComponentSnapshot(sample.getLocalBounds(),true,1.0f),*out));
    }
    // Exercise the real toolbar dialog, including the draft/Apply boundary and revision guard.
    showAdvancedEnvelopeMenu();auto* window=dynamic_cast<juce::DialogWindow*>(juce::ModalComponentManager::getInstance()->getModalComponent(0));
    auto* panel=window?dynamic_cast<AdvancedEnvelopePanel*>(window->getContentComponent()):nullptr;
    check("toolbar_opens_full_tail_fade_dialog",panel!=nullptr);
    if(panel)
    {
        for(int index=0;index<3;++index)
        {
            panel->diagnosticSelectPreview(index);
            const auto regions=panel->diagnosticPreviewRegions();
            const auto edges=pianoRoll.diagnosticRegionEdges((size_t)index);
            const auto absolute=1.0+index*1.4;
            bool aligned=regions.size()==(size_t)(index+2);
            for(size_t i=0;i+1<regions.size();++i)aligned=aligned&&near(regions[i].endSeconds+absolute,edges[i]);
            check("preview_uses_actual_region_count_and_boundaries",aligned);
            check("preview_regions_include_consonant_lead_in",!regions.empty()&&near(regions.front().startSeconds,-.1));
        }
        panel->diagnosticSelectPreview(2);panel->setValues(0,{});
        check("disabled_fade_keeps_four_region_guides",panel->diagnosticPreviewRegions().size()==4);
        {
            juce::PNGImageFormat png;auto out=folder.getChildFile("tail-fade-disabled-four-regions.png").createOutputStream();
            check("disabled_fade_four_regions_snapshot",out&&png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds(),true,1.0f),*out));
        }
        const auto prior=project.contentFingerprint();panel->setValues(2,settings);panel->diagnosticSetControl(4,1.5);
        check("controls_are_draft_until_apply",project.contentFingerprint()==prior);
        panel->diagnosticSetControl(0,95);check("range_controls_keep_order",panel->values().valid()&&panel->values().startFraction<panel->values().endFraction);
        panel->setValues(2,settings);panel->diagnosticSetControl(3,180);check("end_level_cannot_exceed_start",panel->values().valid()&&panel->values().endGain==panel->values().startGain);
        panel->setValues(2,settings);panel->setSize(820,680);check("default_dialog_controls_fit",panel->diagnosticControlsFit());
        juce::PNGImageFormat png;auto out=folder.getChildFile("tail-fade-dialog.png").createOutputStream();check("full_dialog_preview_written",out&&png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds(),true,1.0f),*out));
        panel->setSize(740,660);check("minimum_dialog_controls_fit",panel->diagnosticControlsFit());auto small=folder.getChildFile("tail-fade-dialog-small.png").createOutputStream();check("minimum_dialog_preview_written",small&&png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds(),true,1.0f),*small));
        panel->diagnosticApply();check("dialog_apply_updates_all_selected",project.snapshot().tracks[0].clips[0].notes[2].utauTailFade==settings&&project.snapshot().tracks[0].clips[0].notes[0].utauTailFade==settings);
        panel->setValues(1,{});panel->diagnosticApply();check("repeat_dialog_apply_uses_new_revision",project.snapshot().tracks[0].clips[0].notes[2].utauTailFade==backend::TailFadeSettings{});
        project.setNoteGain("n2",.7f);const auto newer=project.contentFingerprint();panel->setValues(2,settings);panel->diagnosticApply();check("stale_dialog_does_not_overwrite_external_edit",project.contentFingerprint()==newer);window->exitModalState(0);
    }
    // Head envelope follows the actual first OTO region and can run with the tail off.
    for(int n=2;n<=4;++n)
    {
        const auto head=pianoRoll.headEnvelopeSpanFor("n"+juce::String(n));const auto regions=pianoRoll.otoRegionGuidesFor("n"+juce::String(n));
        check("head_span_matches_first_of_2_3_4_regions",head&&!regions.empty()&&near(head->startSeconds,regions.front().startSeconds)&&near(head->endSeconds,regions.front().endSeconds));
    }
    backend::TailFadeSettings both=bezier;both.head.mode=2;both.head.customCurve=true;both.head.startGain=.1;both.head.endGain=.9;
    both.head.control1Time=.2;both.head.control1Progress=.4;both.head.control2Time=.7;both.head.control2Progress=.8;
    auto headLinear=both.head;headLinear.mode=1;headLinear.startGain=0;headLinear.endGain=1;
    check("head_linear_replaces_original_attack",near(backend::headEnvelopeGain(-.05,-.1,0,headLinear,[](double t){return t<-.01?.01f:1.0f;}),.5));
    check("head_leaves_body_at_end_multiplier",near(backend::headEnvelopeGain(.5,-.1,0,both.head,[](double){return .8f;}),.72));
    backend::UtauSampleTiming classicHead;classicHead.preutteranceSeconds=.1;classicHead.consonantSeconds=.08;
    const auto pinnedHead=backend::UtauRenderer::headEnvelopeSpan(classicHead,-.2,1,1,100,false,false);
    check("classic_head_follows_pinned_preutterance",pinnedHead&&near(pinnedHead->startSeconds,-.2)&&near(pinnedHead->endSeconds,-.04));
    backend::TailFadeSettings headOnly;headOnly.head=headLinear;
    const std::vector<AmplitudeEnvelopePoint> headBase{{-.1,-60,true},{-.01,0,true},{1,0,true}};
    const auto headPicture=backend::tailFadePicture(headBase,0,.7,1,headOnly,-.1,0);
    check("head_only_piano_preview_matches_audio",near(backend::envelopeGainFromDb(PianoRollComponent::diagnosticAmplitudeDbAt(headPicture,-.05)),.5));
    check("head_zero_span_keeps_original",near(backend::headEnvelopeGain(0,0,0,both.head,[](double){return .6f;}),.6));
    auto badHead=both;badHead.head.endFraction=badHead.head.startFraction;check("head_invalid_range_rejected",!badHead.valid());
    auto headNote=plain;headNote.utauTailFadeMode=0;headNote.utauTailFade=both;
    check("head_only_invalidates_mix_not_raw",AudioEngine::utauNoteRenderHash(headNote)!=AudioEngine::utauNoteRenderHash(plain)&&AudioEngine::utauNoteAudioHash(headNote)==AudioEngine::utauNoteAudioHash(plain));
    ProjectModel headModel;headModel.resetDocument(data);const auto beforeHead=headModel.contentFingerprint();
    check("head_tail_batch_apply",headModel.setNotesTailFade({"n2","n4"},2,both));
    check("head_tail_single_undo",headModel.undo()&&headModel.contentFingerprint()==beforeHead);headModel.redo();
    check("head_tail_save",headModel.save(folder.getChildFile("head-tail.hjpx"),error));ProjectModel headLoaded;
    check("head_tail_roundtrip",headLoaded.load(folder.getChildFile("head-tail.hjpx"),error)&&headLoaded.snapshot().tracks[0].clips[0].notes[0].utauTailFade==both);
    const auto headLibrary=folder.getChildFile("head-library-"+juce::Uuid().toString()+".json");
    AdvancedEnvelopePanel headPanel({{"head","Head + tail",{.7,1.2},{{-.1,-60,true},{-.08,0,true},{1.2,0,false}}, {},backend::UtauTailFadeSpan{-.1,0}}},1,2,both,false,headLibrary);
    headPanel.diagnosticSection(true);check("switch_to_head_keeps_tail",headPanel.values()==both&&headPanel.mode()==2);
    headPanel.diagnosticSetControl(2,20);headPanel.diagnosticSetControl(3,110);const auto editedBoth=headPanel.values();
    check("head_allows_rising_gain",editedBoth.valid()&&near(editedBoth.head.startGain,.2)&&near(editedBoth.head.endGain,1.1)&&editedBoth.startGain==both.startGain);
    headPanel.diagnosticSection(false);check("switch_to_tail_keeps_head",headPanel.values()==editedBoth);headPanel.diagnosticSection(true);
    check("head_tail_preset_save",headPanel.diagnosticSavePreset("Both ends"));AdvancedEnvelopePanel recalled({},1,0,{},false,headLibrary);recalled.diagnosticPreset(100);
    check("head_tail_preset_recall",recalled.mode()==2&&recalled.values()==editedBoth);
    headPanel.setSize(740,660);check("head_panel_controls_fit",headPanel.diagnosticControlsFit());
    {auto out=folder.getChildFile("head-envelope-panel.png").createOutputStream();juce::PNGImageFormat png;check("head_preview_written",out&&png.writeImageToStream(headPanel.createComponentSnapshot(headPanel.getLocalBounds(),true,1.5f),*out));}
    // Version-1 presets must still load with the head disabled.
    const auto oldLibrary=folder.getChildFile("old-library-"+juce::Uuid().toString()+".json");auto oldRoot=juce::JSON::parse(headLibrary.loadFileAsString());
    oldRoot.getDynamicObject()->setProperty("version",1);oldLibrary.replaceWithText(juce::JSON::toString(oldRoot));std::vector<TailFadePreset> oldRows;
    check("old_presets_default_head_off",TailFadePresetStore(oldLibrary).load(oldRows,error)&&!oldRows.empty()&&oldRows[0].settings.head.mode==0);
    // Test the common native/external mixer callback, not just the mathematical helper.
    backend::UtauRenderRequest request;request.voicebankDirectory=data.tracks[0].voicebankDirectory;request.fourRegion=true;request.consonantClasses=true;request.targetDurationSeconds=2.4;
    backend::UtauNoteRenderSpec spec;spec.alias="a";spec.startSeconds=.5;spec.durationSeconds=1.2;spec.oto=data.tracks[0].clips[0].notes[2].utauOto;spec.amplitudeEnvelope={{0,0,true},{.6,0,false},{1.2,-60,false}};request.notes={spec};
    const auto timing=backend::UtauRenderer::sampleTiming(request.voicebankDirectory,"a",60,100,true,true,&spec.oto);
    const auto span=timing?backend::UtauRenderer::tailFadeSpan(*timing,-.1,1.2,1.2,100,true,true):std::nullopt;check("custom_render_tail_resolves",span.has_value());
    if(span)
    {
        const auto headSpan=backend::UtauRenderer::headEnvelopeSpan(*timing,-.1,1.2,1.2,100,true,true);
        check("actual_render_head_resolves",headSpan.has_value());
        const std::array<double,4> times{headSpan?(headSpan->startSeconds+headSpan->endSeconds)*.5:-.05,span->startSeconds-.02,span->startSeconds+(span->endSeconds-span->startSeconds)*.5,span->startSeconds+(span->endSeconds-span->startSeconds)*.9};
        std::array<float,4> reference{},actual{};
        for(int pass=0;pass<4;++pass)
        {
            request.notes[0].tailFadeMode=pass==0||pass==3?0:2;request.notes[0].tailFadeSettings=pass>=2?both:settings;
            request.notePiece=[&](size_t,const juce::AudioBuffer<float>&,double,double,const std::function<float(double)>& gain,const std::function<float(double)>&){for(size_t i=0;i<times.size();++i)(pass==0?reference:actual)[i]=gain(times[i]);};
            const auto rendered=backend::UtauRenderer::render(request);check("custom_actual_renderer_produces_audio",rendered.buffer.getNumSamples()>0);
            if(pass>0)for(size_t i=0;i<times.size();++i)check(pass==3?"head_only_actual_mixer_matches_preview":pass==2?"bezier_actual_mixer_matches_preview":"smooth_actual_mixer_matches_preview",near(actual[i],backend::advancedEnvelopeGain(request.notes[0].tailFadeMode,times[i],span->startSeconds,span->endSeconds,headSpan?headSpan->startSeconds:0,headSpan?headSpan->endSeconds:0,request.notes[0].tailFadeSettings,[&](double t){
                return backend::envelopeGainFromDb(t<=.6?0.0f:backend::envelopeDbBetween(0,-60,(float)((t-.6)/.6),false));})));
        }
        for(size_t i=0;i<times.size();++i)check("custom_actual_mixer_matches_preview",near(actual[i],backend::advancedEnvelopeGain(0,times[i],span->startSeconds,span->endSeconds,headSpan?headSpan->startSeconds:0,headSpan?headSpan->endSeconds:0,both,[&](double t){
            const auto db=t<=.6?0.0f:backend::envelopeDbBetween(0,-60,(float)((t-.6)/.6),false);
            return backend::envelopeGainFromDb(db);
        })));
    }
    return ok;
}
}
