#pragma once
#include "MainProjectRegistrationUiSmoke.h" // shared Windows message-pump declarations
#include "../MelodyneTrackSelector.h"

namespace hachi
{
// Drives the production offer/confirmation/folder-result/cancel handlers. No
// second registration planner, fake model commit or alternative OTTO writer.
inline bool MainComponent::diagnosticRegistrationJapaneseChoiceUi(const juce::File& output)
{
#if !JUCE_WINDOWS
    juce::ignoreUnused(output);return false;
#else
    cancelProjectMaterialRegistration();audio.stop();stopTimer();
    const auto root=output.getChildFile("registration-japanese-choice-"+juce::Uuid().toString());
    if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";};
    const auto pump=[]
    {
        MSG message{};
        for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i)
        {if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return;}TranslateMessage(&message);DispatchMessageW(&message);}
    };
    const auto wait=[&](const std::function<bool()>& condition,int timeout=15000)
    {
        const auto end=juce::Time::getMillisecondCounterHiRes()+timeout;
        while(!condition()&&juce::Time::getMillisecondCounterHiRes()<end){pump();juce::Thread::sleep(5);}
        pump();return condition();
    };
    const auto prompt=[]()->juce::AlertWindow*
    {
        auto* p=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        return p&&p->getComponentID()=="project-material-registration-confirm"?p:nullptr;
    };
    const auto choice=[&]()->juce::ToggleButton*
    {auto* p=prompt();return p?dynamic_cast<juce::ToggleButton*>(p->getCustomComponent(0)):nullptr;};
    const auto source=root.getChildFile("recording.wav");
    juce::AudioBuffer<float> wave(1,48000);wave.clear();
    auto stream=source.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),48000,1,24,{},0):nullptr);
    check("source_fixture",writer&&writer->writeFromAudioSampleBuffer(wave,0,48000));writer.reset();
    ProjectData seed;seed.name="registration-japanese-choice";
    TrackData track;track.id="track";track.pitchAlgorithm=PitchAlgorithm::world;
    ClipData clip;clip.id="owner";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=1;clip.sourceTimeMap={{0,0},{1,1}};
    NoteData note;note.id="note";note.label="KEEP";note.durationSeconds=1;note.midiNote=note.sourceMidiCenter=60;
    note.nativeRole=NativeSegmentRole::vowel;note.nativeSourceStartSeconds=0;note.nativeSourceEndSeconds=1;
    clip.notes={note};track.clips={clip};seed.tracks={track};project.resetDocument(seed);
    const auto before=project.contentFingerprint();const auto revision=project.revisionNumber();
    MelodyneImportOptimizationOptions oldOptions;oldOptions.autoJapanesePronunciation=true;
    MelodyneTrackSelector selector(seed.tracks,strings,oldOptions);
    bool hasIndependentJapanese=false;
    for(int i=0;i<selector.getNumChildComponents();++i)
        hasIndependentJapanese|=selector.getChildComponent(i)->getComponentID()=="melodyne-auto-japanese-pronunciation";
    check("track_selector_cannot_enable_registration_recognition",!hasIndependentJapanese&&!selector.getOptions().autoJapanesePronunciation);
    int folderRequests=0;std::function<void(const juce::File&)> folderResult;
    projectRegistrationFolderPickerForTest=[&](auto callback){++folderRequests;folderResult=std::move(callback);};
    projectRegistrationJapaneseModelRootForTest=[root]{return root.getChildFile("absent-models");};
    offerMaterialFolderForImport(project.snapshot(),true);
    check("explicit_registration_has_default_off_checkbox",wait([&]{return choice()!=nullptr;})&&choice()&&!choice()->getToggleState());
    if(auto* toggle=choice())toggle->setToggleState(true,juce::dontSendNotification);
    if(auto* p=prompt())p->exitModalState(0);
    check("skip_with_checkbox_never_starts_folder_or_recognition",wait([&]{return !projectRegistrationOperation;})&&folderRequests==0&&project.contentFingerprint()==before);
    offerMaterialFolderForImport(project.snapshot(),false);
    check("automatic_offer_stays_nonmodal",wait([&]{return registrationInfoBar.isVisible();})&&prompt()==nullptr);
    if(registrationInfoRegister.onClick)registrationInfoRegister.onClick();
    check("automatic_register_opens_same_confirmation_before_picker",wait([&]{return choice()!=nullptr;})&&folderRequests==0&&choice()&&!choice()->getToggleState());
    if(auto* toggle=choice())toggle->setToggleState(true,juce::dontSendNotification);
    if(auto* p=prompt())p->exitModalState(1);
    check("confirmed_registration_alone_reaches_picker",wait([&]{return bool(folderResult);})&&folderRequests==1);
    if(folderResult)folderResult(root);
    check("recognition_has_explicit_cancel_action",registrationInfoBar.isVisible()&&registrationInfoRegister.getButtonText()==strings.text("registration.cancelRecognition"));
    if(registrationInfoRegister.onClick)registrationInfoRegister.onClick();
    pump();
    check("cancel_joins_owned_worker_without_publishing_or_adopting",!projectRegistrationOperation&&project.contentFingerprint()==before&&project.revisionNumber()==revision);
    juce::Array<juce::File> manifests;root.findChildFiles(manifests,juce::File::findFiles,true,"otto.ini");
    check("cancel_has_no_half_otto",manifests.isEmpty());
    cancelProjectMaterialRegistration();projectRegistrationFolderPickerForTest={};projectRegistrationJapaneseModelRootForTest={};
    return root.getChildFile("registration-japanese-choice-ui.txt").replaceWithText(log)&&ok;
#endif
}
}
