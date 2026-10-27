#pragma once
#include "../NativeTransitionDetection.h"
#include "../OtoWaveformEditorComponent.h"
#include "../SettingsComponent.h"
#include "../OttoMaterialIO.h"
#include "TeioProjectMaterialRegistrationAudioGate.h"
#include <iostream>
namespace hachi
{
inline bool runNativeTransitionDetectionSmoke(const juce::File& output,const juce::File& inspectProject={})
{
    const auto root=output.getChildFile("transition-detection-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    struct RootsRestore{juce::StringArray roots;~RootsRestore(){SampleSettings::setMaterialRoots(roots);}} restoreRoots{SampleSettings::materialRoots()};SampleSettings::setMaterialRoots({root.getFullPathName()});
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+(error.isEmpty()?juce::String{}:" | "+error)+"\n";std::cout<<name<<"="<<pass;if(error.isNotEmpty())std::cout<<" | "<<error;std::cout<<std::endl;error.clear();};
    const auto source=root.getChildFile("unrelated_filename.wav");
    {juce::AudioBuffer<float> pcm(1,48000);pcm.clear();for(int i=12000;i<17760;++i){const auto local=(i-12000)/5760.;const auto gain=std::abs(local-.5)<.17?.025:.6;pcm.setSample(0,i,float((i%2?1:-1)*gain));}juce::WavAudioFormat format;auto out=source.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(out.release(),48000,1,24,{},0));check("controlled_short_U_energy_slice",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));}
    ProjectData original;TrackData track;track.id="melodyne";track.trackType=TrackType::hachi;track.compose=true;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;ClipData clip;clip.id="long-recording";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceTimeMap={{0,0},{1,1}};NoteData note;note.id="bridge";note.label="-";note.startSeconds=.25;note.durationSeconds=.12;note.consonantSeconds=.01;note.nativeUnpitched=true;note.contour={{0,0,0,false},{.12,0,0,false}};clip.notes={note};track.clips={clip};original.tracks={track};
    const auto candidates=detectMelodyneTransitions(original);check("short_fully_cut_long_source_slice_detected",candidates.size()==1&&candidates[0].shape.boundarySourceSeconds>.28&&candidates[0].shape.boundarySourceSeconds<.34);
    auto joined=original;joined.tracks[0].clips[0].notes[0].connectedToNext=true;check("partial_pitch_join_never_auto_transition",detectMelodyneTransitions(joined).empty());
    auto longNote=original;longNote.tracks[0].clips[0].notes[0].durationSeconds=.3;check("long_sound_not_short_transition",detectMelodyneTransitions(longNote).empty());
    auto stable=original;stable.tracks[0].clips[0].notes[0].nativeUnpitched=false;stable.tracks[0].clips[0].notes[0].contour={{0,0,0,true},{.12,10,0,true}};check("stable_voiced_short_vowel_is_ambiguous_not_registered",detectMelodyneTransitions(stable).empty());
    auto direct=original;direct.tracks[0].clips[0].sourceOffsetSeconds=.25;direct.tracks[0].clips[0].durationSeconds=direct.tracks[0].clips[0].sourceDurationSeconds=.12;direct.tracks[0].clips[0].sourceTimeMap={{0,0},{.12,.12}};direct.tracks[0].clips[0].notes[0].startSeconds=0;check("independent_source_range_uses_absolute_middle",detectMelodyneTransitions(direct).size()==1);
    auto off=original,on=original;juce::StringArray warnings;const bool offSaved=SampleSettings::convertMelodyneProject(off,warnings,false);std::cout<<"off_saved="<<offSaved<<" warnings="<<warnings.joinIntoString(" | ")<<std::endl;warnings.clear();const bool onSaved=SampleSettings::convertMelodyneProject(on,warnings,true);std::cout<<"on_saved="<<onSaved<<" warnings="<<warnings.joinIntoString(" | ")<<" manifest="<<SampleSettings::annotationFor(source).getFullPathName()<<std::endl;
    check("import_toggle_registers_kind_and_middle_only_when_enabled",off.tracks[0].clips[0].notes[0].pronunciationKind==PronunciationKind::normal&&on.tracks[0].clips[0].notes[0].pronunciationKind==PronunciationKind::transition&&on.tracks[0].clips[0].notes[0].nativeTransition);
    check("auto_identity_leaves_complete_native_render_request_identical",teioRegistrationAudioDetail::requestFingerprint(off)==teioRegistrationAudioDetail::requestFingerprint(on));
    check("auto_registration_does_not_activate_new_edge_gains_on_imported_snapshot",!on.tracks[0].clips[0].notes[0].nativePronunciationEdges&&on.tracks[0].clips[0].notes[0].consonantSeconds==off.tracks[0].clips[0].notes[0].consonantSeconds);
    OttoMaterialIO::Document autoDocument;check("registered_source_has_independent_non_CV_shape_and_fade_defaults",OttoMaterialIO::loadDocument(root.getChildFile("otto.ini"),autoDocument,error)&&autoDocument.audio[0].rows[0].nativeTransition&&autoDocument.audio[0].rows[0].fixedDurationSeconds==0&&autoDocument.audio[0].rows[0].alignmentSeconds==.25&&autoDocument.audio[0].rows[0].nativePronunciationEdges);
    // Real precise-editor kind selection and middle input/drag, not a second editor.
    warnings.clear();SampleSettings::convertMelodyneProject(off,warnings,false);OttoMaterialIO::Document normalDocument;OttoMaterialIO::loadDocument(root.getChildFile("otto.ini"),normalDocument,error);
    if(normalDocument.audio.empty()||normalDocument.audio[0].rows.empty()){root.getChildFile("verification.txt").replaceWithText(log+"normal_document_error="+error+" warnings="+warnings.joinIntoString(" | "));return false;}
    I18n strings;strings.setLanguage(I18n::Language::enUS);auto editor=OtoWaveformEditorComponent::forMaterial(source,root.getChildFile("otto.ini"),strings,[](const auto&){},false,&error,normalDocument.audio[0].id);
    check("actual_precise_editor_open",editor&&editor->selectMaterialPronunciation(normalDocument.audio[0].rows[0].materialRegionId,true));
    const auto find=[&](auto&& self,juce::Component& c,const juce::String& id)->juce::Component*{if(c.getComponentID()==id)return &c;for(int i=0;i<c.getNumChildComponents();++i)if(auto* result=self(self,*c.getChildComponent(i),id))return result;return nullptr;};
    if(editor)
    {
        editor->diagnosticSetAnalysisLayers(false,false);editor->diagnosticPronunciationKind(2);editor->setSize(1180,850);editor->resized();auto* middle=dynamic_cast<juce::TextEditor*>(find(find,*editor,"material.transitionBoundary"));
        check("transition_hides_C_P_O_and_vowel_controls",middle&&middle->isVisible()&&!editor->diagnosticParameterShown(1)&&!editor->diagnosticParameterShown(3)&&!editor->diagnosticParameterShown(4)&&editor->diagnosticMaterialVowelItems().isEmpty());
        if(middle){middle->setText("315",false);if(middle->onReturnKey)middle->onReturnKey();editor->diagnosticDragMaterialBoundary(0,.31);check("middle_numeric_and_existing_native_boundary_drag_agree",std::abs(middle->getText().getDoubleValue()-310)<1.e-6);}
        editor->diagnosticSave();OttoMaterialIO::Document saved;check("editor_source_save_cold_middle_no_C_or_P",OttoMaterialIO::loadDocument(root.getChildFile("otto.ini"),saved,error)&&saved.audio[0].rows[0].nativeTransition&&std::abs(saved.audio[0].rows[0].nativeTransition->boundarySourceSeconds-.31)<1.e-8&&saved.audio[0].rows[0].fixedDurationSeconds==0&&saved.audio[0].rows[0].alignmentSeconds==.25);
        juce::Image image(juce::Image::ARGB,1180,850,true,juce::SoftwareImageType{});juce::Graphics g(image);editor->paintEntireComponent(g,true);juce::PNGImageFormat png;if(auto out=root.getChildFile("transition-precise-editor.png").createOutputStream())png.writeImageToStream(image,*out);
    }
    juce::PropertiesFile::Options opts;opts.applicationName="transition-option";juce::PropertiesFile props(root.getChildFile("prefs.xml"),opts);juce::AudioDeviceManager devices;SettingsComponent settings(strings,devices,props,[]{});settings.selectPage(4);auto* toggle=dynamic_cast<juce::ToggleButton*>(find(find,settings,"settings-auto-register-transitions"));check("real_import_option_defaults_enabled",toggle&&toggle->getToggleState());
    if(inspectProject.existsAsFile()){ProjectModel model;if(model.load(inspectProject,error)){const auto actual=detectMelodyneTransitions(model.snapshot());log+="inspected_project="+inspectProject.getFileName()+" candidates="+juce::String(actual.size())+"\n";for(const auto& c:actual)log+=c.noteId+" middle="+juce::String(c.shape.boundarySourceSeconds,6)+" confidence="+juce::String(c.confidence,3)+"\n";}else check("requested_project_read_only_load",false);}
    root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_transition_detection_fixture="<<root.getFullPathName()<<std::endl;return ok;
}
}
