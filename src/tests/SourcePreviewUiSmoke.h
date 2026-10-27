#pragma once
#include "../OtoWaveformEditorComponent.h"
#include "../OttoMaterialIO.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
bool MainComponent::diagnosticSourcePreviewUi(const juce::File& output)
{
    stopTimer();stopOtoPreviews();audio.devices().closeAudioDevice();audio.prepareToPlay(257,48000);sourcePreviewOfflineForTest=true;
    const auto base=output.getChildFile("source-preview-"+juce::Uuid().toString());const auto source=base.getChildFile("one.wav"),manifest=base.getChildFile("otto.ini");
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";};
    if(base.createDirectory().failed())return false;
    juce::AudioBuffer<float> pcm(1,48000);for(int i=0;i<48000;++i)pcm.setSample(0,i,.31f*std::sin(juce::MathConstants<double>::twoPi*(143+i/2000.0)*i/48000.0));
    auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);
    check("raw_PCM_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,48000));writer.reset();
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    juce::AudioBuffer<float> reference(1,48000);check("decoded_raw_reference",reader&&reader->read(&reference,0,48000,0,true,false));
    SampleRegionSetting a;a.name="a";a.materialAudioId="variant-A";a.materialRegionId="same-row-id";a.role=NativeSegmentRole::vowel;
    a.regionStartSeconds=a.alignmentSeconds=.1;a.regionEndSeconds=.3;auto b=a;b.name="b";b.materialAudioId="variant-B";b.regionStartSeconds=b.alignmentSeconds=.4;b.regionEndSeconds=.7;
    OttoMaterialIO::Document document;document.audio={{a.materialAudioId,source.getFileName(),{a}},{b.materialAudioId,source.getFileName(),{b}}};
    check("same_file_two_stable_variants",OttoMaterialIO::saveDocument(manifest,document,error));if(!ok)return false;
    ProjectData seed;seed.bpm=137;TrackData track;track.id="preview-music";seed.tracks.push_back(track);project.resetDocument(seed);project.dispatchPendingMessages();
    const auto music=project.contentFingerprint();const auto revision=project.revisionNumber();const auto sourceHash=juce::SHA256(source).toHexString(),manifestHash=juce::SHA256(manifest).toHexString();
    setAssetManagerVisible(true);check("real_Manager_root_and_preview_callback",assetManager&&assetManager->registerMaterialRoot(base,error)&&bool(assetManager->onPreviewSource));
    if(!assetManager)return false;assetManager->diagnosticToggleAudio(source,a.materialAudioId);assetManager->diagnosticToggleAudio(source,b.materialAudioId);
    const auto click=[&](juce::Rectangle<int> bounds,int clicks=1)
    {
        const auto point=bounds.getCentre().toFloat();return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),0,0,0,0,0,assetManager.get(),assetManager.get(),juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),clicks,false);
    };
    const auto pull=[&](double start,double end)
    {
        const int first=juce::roundToInt(start*48000),count=juce::roundToInt((end-start)*48000);int read=0;bool exact=true;
        while(audio.isPlaying()&&read<60000)
        {
            juce::AudioBuffer<float> block(2,257);block.clear();audio.getNextAudioBlock({&block,0,257});
            for(int i=0;i<257;++i){const float expected=read+i<count?reference.getSample(0,first+read+i):0.0f;exact&=std::abs(block.getSample(0,i)-expected)<1.e-6f&&std::abs(block.getSample(1,i)-expected)<1.e-6f;}
            read+=257;
        }
        return exact&&!audio.isPlaying()&&std::abs(audio.position()-end)<1.e-7&&read>=count&&read<count+257;
    };
    const auto full=assetManager->diagnosticPreviewButtonBounds(source,a.materialAudioId,{});
    const auto regionA=assetManager->diagnosticPreviewButtonBounds(source,a.materialAudioId,a.materialRegionId),regionB=assetManager->diagnosticPreviewButtonBounds(source,b.materialAudioId,b.materialRegionId);
    check("file_and_pronunciation_buttons_exist",!full.isEmpty()&&!regionA.isEmpty()&&!regionB.isEmpty());
    const auto selection=juce::JSON::toString(assetManager->diagnosticTreeSnapshot());audio.setPosition(2.75);assetManager->mouseDown(click(full));
    check("file_button_exact_whole_recording_PCM_and_EOF",sourcePreviewActive&&pull(0,1));timerCallback();
    check("whole_preview_restores_song_position_no_resume",!sourcePreviewActive&&!audio.isPlaying()&&std::abs(audio.position()-2.75)<1.e-7);
    assetManager->mouseDown(click(regionB));check("pronunciation_exact_variant_B_absolute_range_PCM",sourcePreviewActive&&pull(.4,.7));timerCallback();
    assetManager->mouseDown(click(regionA));check("same_region_ID_variant_A_not_first_or_nearest",sourcePreviewActive&&pull(.1,.3));timerCallback();
    assetManager->mouseDown(click(regionA,2));assetManager->mouseDoubleClick(click(regionA,2));assetManager->mouseDrag(click(regionA));
    check("preview_hits_no_selection_expand_load_or_edit",juce::JSON::toString(assetManager->diagnosticTreeSnapshot())==selection&&!sourcePreviewActive&&project.contentFingerprint()==music&&project.revisionNumber()==revision);
    sourceEditActive=true;sampleSettingsFile=source;audio.setAuditionFile(source);audio.setPosition(.82);assetManager->mouseDown(click(regionA));
    check("source_edit_preview_PCM",pull(.1,.3));timerCallback();check("source_edit_audition_position_restored",sourceEditActive&&!audio.isPlaying()&&std::abs(audio.position()-.82)<1.e-7);
    sourceEditActive=false;audio.clearAuditionFile();
    // Production OTO host delegates to the same Main raw transport; standalone
    // fallback is separately pulled to prove cropping does not synthesize audio.
    auto editor=OtoWaveformEditorComponent::forMaterial(source,manifest,strings,[](const auto&){},false,&error,b.materialAudioId);
    check("shared_editor_exact_variant",bool(editor));if(editor)
    {
        check("overview_row_select",editor->selectMaterialPronunciation(b.materialRegionId,false));attachOtoPlayback(*editor);editor->diagnosticPressPlay();
        check("production_overview_uses_common_raw_range",sourcePreviewActive&&pull(.4,.7));timerCallback();editor->stopPlaybackPreview();
        editor->selectMaterialPronunciation(b.materialRegionId,true);editor->diagnosticZoomIn();editor->diagnosticScrollTo(500);editor->diagnosticPressPlay();
        check("detail_preview_row_range_not_waveform_zoom",sourcePreviewActive&&pull(.4,.7));timerCallback();editor->stopPlaybackPreview();
        editor->setPlaybackHost({});editor->diagnosticPressPlay();juce::AudioBuffer<float> result;
        const int count=editor->diagnosticPullPreview(result,48000,1);bool exact=count==14400;
        for(int i=0;i<count&&i<14400;++i)exact&=std::abs(result.getSample(0,i)-reference.getSample(0,19200+i))<1.e-6f;
        check("standalone_existing_SourcePreview_crops_exact_PCM",exact);
    }
    check("all_preview_actions_preserve_PCM_OTTO_music",juce::SHA256(source).toHexString()==sourceHash&&juce::SHA256(manifest).toHexString()==manifestHash&&project.contentFingerprint()==music&&project.revisionNumber()==revision);
    // Keep stale painted row/button intentionally: exact click-time resolution
    // must refuse deleted variant B's row, never fall back to A's shared row ID.
    OttoMaterialIO::Document updated;OttoMaterialIO::loadDocument(manifest,updated,error);updated.audio.erase(std::remove_if(updated.audio.begin(),updated.audio.end(),[&](const auto& entry){return entry.id==b.materialAudioId;}),updated.audio.end());
    check("external_variant_delete_fixture",OttoMaterialIO::saveDocument(manifest,updated,error));assetManager->mouseDown(click(regionB));check("deleted_stable_variant_no_fallback_play",!sourcePreviewActive);
    if(auto* warning=juce::Component::getCurrentlyModalComponent())warning->exitModalState(0);
    stopSourcePreview();sourcePreviewOfflineForTest=false;
    const auto written=base.getChildFile("verification.txt").replaceWithText(log);std::cout<<"source_preview_ui="<<ok<<"|fixture="<<base.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return ok&&written;
}
}
