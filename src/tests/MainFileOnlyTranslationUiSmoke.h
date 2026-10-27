#pragma once
#include "../PronunciationCandidateBox.h"
#include "../DiffSingerPronunciationEditor.h"
#if JUCE_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
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
bool MainComponent::diagnosticFileOnlyTranslationUi(const juce::File& directory)
{
    if(directory.createDirectory().failed())return false;bool ok=true;juce::String report;
    const auto check=[&](const juce::String& key,bool pass){ok&=pass;report<<key<<"="<<(pass?"PASS":"FAIL")<<"\n";};
    const auto pump=[]
    {
#if JUCE_WINDOWS
        for(int turn=0;turn<4;++turn){MSG message{};for(int i=0;i<200&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));return;}TranslateMessage(&message);DispatchMessageW(&message);}juce::Thread::sleep(2);}
#endif
    };
    const auto picture=[&](juce::Component& content,const juce::String& name)
    {const auto image=content.createComponentSnapshot(content.getLocalBounds(),true,1.f);auto stream=directory.getChildFile(name+".png").createOutputStream();juce::PNGImageFormat png;return stream&&png.writeImageToStream(image,*stream);};
    I18n::clearMissingTranslationKeys();
    for(int language=1;language<=5;++language)
    {
        settingsWindows.clear();preferences->setValue("ui.language",language);applyPreferences();refreshTexts();setSize(1280,800);
        const auto tag="language-"+juce::String(language);
        showSettings(0);pump();auto* window=settingsWindows.findVisible<SettingsComponent>();
        check(tag+"_real_settings_window",window!=nullptr);
        if(window)
        {
            auto* settings=static_cast<SettingsComponent*>(window->getContentComponent());
            for(int page:{0,1,2,3,4,5}){settings->selectPage(page);pump();check(tag+"_settings_page_"+juce::String(page),picture(*settings,tag+"-settings-"+juce::String(page)));}
        }
        settingsWindows.clear();
        PronunciationCandidateBox picker(strings,{},[](const auto&){},[]{});picker.setSize(720,420);
        check(tag+"_material_picker",picture(picker,tag+"-material-picker"));
        DiffSingerPronunciationEditor pronunciation(juce::var(juce::Array<juce::var>{}),{}, {},[](const auto&,const auto&,bool,auto){});
        pronunciation.setSize(1000,600);check(tag+"_DS_pronunciation",picture(pronunciation,tag+"-DS-pronunciation"));
        showHamood();pump();auto* harmony=juce::Component::getCurrentlyModalComponent();check(tag+"_harmony_panel",harmony!=nullptr);
        if(harmony){check(tag+"_harmony_picture",picture(*harmony,tag+"-harmony"));harmony->exitModalState(0);pump();}
        check(tag+"_new_component_keeps_selected_language",I18n{}.getLanguage()==I18n::Language(language-1));
    }
    const auto missing=I18n::missingTranslationKeys();directory.getChildFile("missing-keys.json").replaceWithText(juce::JSON::toString(juce::var(missing),true));
    check("all_opened_UI_translation_values_are_in_the_file",missing.isEmpty());report<<"missing_keys="<<missing.joinIntoString(" | ")<<"\n";
    directory.getChildFile("verification.txt").replaceWithText(report);std::cout<<report<<std::endl;return ok;
}
}
