#pragma once
#include "../VoicebankSettingsComponent.h"
#include "../OtoWaveformEditorComponent.h"
#include "../backend/UstText.h"
namespace hachi
{
inline bool MainComponent::diagnosticOtoContinuity(const juce::File& folder,const juce::File& profileBank)
{
    folder.createDirectory();stopTimer();bool ok=true;
    const auto check=[&](const char* name,bool value){ok=ok&&value;std::cout<<name<<'='<<value<<std::endl;};
    const auto modal=[&](bool editor)->juce::DialogWindow*
    {
        auto* list=voicebankWindows.findVisible<VoicebankSettingsComponent>();
        return editor&&list?dynamic_cast<VoicebankSettingsComponent*>(list->getContentComponent())->diagnosticEditorWindow():list;
    };
    for(const auto& source:std::vector<juce::String>{"", "\n", "\r\n", "\r", "a\r\nb\nc\rd", "a\n\n", juce::String::fromUTF8("调音🎵=a\r\n声母=zh\n韵尾=ng")})
    {
        juce::String rebuilt;for(const auto& line:backend::usttext::lines(source))rebuilt+=line.body+line.ending;
        check("line_scanner_preserves_unicode_and_mixed_line_endings",rebuilt==source);
    }
    const auto wave=folder.getChildFile("tone.wav");
    {
        juce::AudioBuffer<float> samples(1,22050);
        for(int i=0;i<samples.getNumSamples();++i)samples.setSample(0,i,.15f*(float)std::sin(juce::MathConstants<double>::twoPi*220*i/44100));
        auto stream=wave.createOutputStream();juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),44100,1,16,{},0));
        check("continuity_audio_fixture",writer&&writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples()));
    }
    for(int mode=0;mode<3;++mode)
    {
        const auto bank=folder.getChildFile("bank-"+juce::String(mode));bank.createDirectory();
        wave.copyFileTo(bank.getChildFile("tone.wav"));wave.copyFileTo(bank.getChildFile("orphan.wav"));
        juce::String text;
        for(int i=0;i<1031;++i)text+="tone.wav="+(i<2?juce::String():i<6?juce::String("duplicate"):"phoneme_"+juce::String(i).paddedLeft('0',4))+",0,100,-400,50,20\n";
        bank.getChildFile("oto.ini").replaceWithText(text);
        ProjectData data;TrackData track;track.id="oto-track";track.name="OTO continuity";track.pitchAlgorithm=PitchAlgorithm::utau;
        track.utauMode=mode==0?UtauMode::classic:mode==1?UtauMode::jie:UtauMode::mou;track.voicebankDirectory=bank;data.tracks.push_back(track);
        project.resetDocument(data);project.dispatchPendingMessages();stopTimer();selectedTrackId=track.id;selectedNoteId={};
        showVoicebankSettings();auto* listWindow=modal(false);
        auto* panel=listWindow?dynamic_cast<VoicebankSettingsComponent*>(listWindow->getContentComponent()):nullptr;
        check("real_voicebank_dialog_opens",panel!=nullptr);if(!panel)continue;
        panel->setSize(1000,600);
        check("thousand_rows_and_unregistered_sample_loaded",panel->diagnosticRowCount()==1032);
        for(const int row:{850,851,3,1})
        {
            panel->diagnosticSelectRow(row);panel->diagnosticScrollTo(35,std::max(0,row*22-120));
            const auto selected=panel->diagnosticSelectedEntry();const auto position=panel->diagnosticScrollPosition();
            panel->diagnosticOpenEditor();auto* editorWindow=modal(true);
            auto* editor=editorWindow?dynamic_cast<OtoWaveformEditorComponent*>(editorWindow->getContentComponent()):nullptr;
            check("selected_entry_opens_editor",editor!=nullptr);if(!editor)continue;
            check("list_entrypoint_has_live_audio_host",editor->diagnosticHasPlaybackHost());
            // Pull through the actual playback source without opening a physical device.
            juce::AudioDeviceManager devices;int starts=0;
            editor->setPlaybackHost({[&]{return &devices;},[&]{++starts;return true;}});
            editor->diagnosticPressPlay();check("list_preview_attaches_audio_callback",editor->diagnosticPreviewAttached()&&starts==1);
            juce::AudioBuffer<float> output;editor->diagnosticPullPreview(output,48000,.1);
            check("list_preview_contains_original_audio",output.getNumSamples()>0&&output.getMagnitude(0,output.getNumSamples())>.05f);
            editor->diagnosticPressPlay();check("list_preview_stops_and_detaches",!editor->diagnosticPreviewAttached());
            editor->diagnosticDragOffsetTo(15);
            if(row==850)
            {
                juce::StringArray warnings;const auto begin=juce::Time::getMillisecondCounterHiRes();
                SampleSettings::loadVoicebankOto(bank,warnings,mode!=0,mode==2,selected->otoFile);
                std::cout<<"parse_"<<mode<<"_ms="<<juce::Time::getMillisecondCounterHiRes()-begin<<std::endl;
                const auto refresh=juce::Time::getMillisecondCounterHiRes();panel->diagnosticReloadFile(selected->otoFile);
                std::cout<<"refresh_"<<mode<<"_ms="<<juce::Time::getMillisecondCounterHiRes()-refresh<<std::endl;
            }
            const auto started=juce::Time::getMillisecondCounterHiRes();editor->diagnosticSave();
            std::cout<<"save_"<<mode<<"_"<<row<<"_ms="<<juce::Time::getMillisecondCounterHiRes()-started<<std::endl;
            const auto after=panel->diagnosticSelectedEntry();
            check("save_keeps_exact_entry_including_duplicate_or_empty_alias",after&&selected&&after->lineIndex==selected->lineIndex&&after->alias==selected->alias);
            check("save_keeps_both_scroll_axes",panel->diagnosticScrollPosition()==position);
            check("save_refreshes_timing_and_fingerprint",after&&std::abs(after->offsetMs-15)<.001&&after->sourceFingerprint!=selected->sourceFingerprint);
        }
        panel->diagnosticFilter("phoneme_08");panel->diagnosticSelectRow(50);panel->diagnosticScrollTo(30,750);
        const auto position=panel->diagnosticScrollPosition();const auto selected=panel->diagnosticSelectedEntry();const auto count=panel->diagnosticRowCount();
        panel->diagnosticOpenEditor();if(auto* window=modal(true))
        {auto* editor=dynamic_cast<OtoWaveformEditorComponent*>(window->getContentComponent());editor->diagnosticDragOffsetTo(25);editor->diagnosticSave();}
        check("save_keeps_search_filter_and_row",panel->diagnosticRowCount()==count&&panel->diagnosticSelectedEntry()->lineIndex==selected->lineIndex);
        check("filtered_save_keeps_scroll",panel->diagnosticScrollPosition()==position);
        if(mode==2)
        {
            panel->diagnosticSetRegionCount(3);
            check("inline_region_change_keeps_selection_and_scroll",panel->diagnosticSelectedEntry()->lineIndex==selected->lineIndex&&panel->diagnosticScrollPosition()==position&&panel->diagnosticToggledCount()==3);
        }
        panel->diagnosticFilter("orphan.wav");check("unregistered_row_is_editable",panel->diagnosticRowCount()==1&&panel->diagnosticSelectedEntry()->lineIndex<0);
        panel->diagnosticOpenEditor();if(auto* window=modal(true))dynamic_cast<OtoWaveformEditorComponent*>(window->getContentComponent())->diagnosticSave();
        check("newly_registered_row_stays_selected",panel->diagnosticRowCount()==1&&panel->diagnosticSelectedEntry()->sourceName=="orphan.wav"&&panel->diagnosticSelectedEntry()->lineIndex>=0);
        // A targeted refresh must not rescan an unrelated newly added directory.
        const auto unrelated=bank.getChildFile("added-later");unrelated.createDirectory();unrelated.getChildFile("oto.ini").replaceWithText("missing.wav=newly-added,0,0,0,0,0\n");
        panel->diagnosticFilter({});const auto beforeCount=panel->diagnosticRowCount();const auto activeFile=panel->diagnosticSelectedEntry()->otoFile;
        panel->diagnosticReloadFile(activeFile);check("save_refresh_avoids_unrelated_directory_scan",panel->diagnosticRowCount()==beforeCount);
        panel->diagnosticSelectRow(850);
        juce::PNGImageFormat png;auto out=folder.getChildFile("saved-list-"+juce::String(mode)+".png").createOutputStream();
        check("saved_list_snapshot",out&&png.writeImageToStream(panel->createComponentSnapshot(panel->getLocalBounds(),true,1),*out));
        listWindow->closeButtonPressed();voicebankWindows.removeClosed();
    }
    if(profileBank.isDirectory())
    {
        const auto start=juce::Time::getMillisecondCounterHiRes();
        VoicebankSettingsComponent panel(profileBank,true,false);panel.setSize(1000,600);panel.diagnosticSelectRow(std::min(800,panel.diagnosticRowCount()-1));
        std::cout<<"real_bank_full_open_ms="<<juce::Time::getMillisecondCounterHiRes()-start<<std::endl;
        if(auto entry=panel.diagnosticSelectedEntry())
        {
            const auto position=panel.diagnosticScrollPosition();const auto begin=juce::Time::getMillisecondCounterHiRes();
            panel.diagnosticReloadFile(entry->otoFile);
            std::cout<<"real_bank_partial_refresh_ms="<<juce::Time::getMillisecondCounterHiRes()-begin<<std::endl;
            check("real_bank_readonly_refresh_preserves_entry",panel.diagnosticSelectedEntry()->lineIndex==entry->lineIndex&&panel.diagnosticScrollPosition()==position);
        }
    }
    return ok;
}
}
