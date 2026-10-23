#pragma once
#include "../MainComponent.h"
#include "ComponentExportSmoke.h"
#include <iostream>

namespace hachi
{
inline void MainComponent::diagnosticWavExport(const juce::File& folder, std::function<void(bool)> done)
{
    folder.createDirectory();
    stopTimer();
    setSize(1400, 860);
    struct Report { bool ok = true; juce::Array<juce::var> checks; };
    auto report = std::make_shared<Report>();
    const auto check = [report](juce::String name, bool passed)
    {
        auto* item = new juce::DynamicObject(); item->setProperty("name", name); item->setProperty("passed", passed);
        report->checks.add(item); report->ok = report->ok && passed;
        std::cout << name << "=" << passed << std::endl;
    };
    const auto finish = [report, folder, done]
    {
        auto* result = new juce::DynamicObject();result->setProperty("passed",report->ok);result->setProperty("checks",report->checks);
        folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(result),true));done(report->ok);
    };
    // Native window destruction may dispatch timer messages before its modal
    // callback finishes. Wait for completion, never assume a 100 ms delay.
    struct Await final : juce::Timer
    {
        std::function<bool()> ready;
        std::function<void()> action, timeout;
        int remaining = 200;
        Await(std::function<bool()> r, std::function<void()> a, std::function<void()> t)
            : ready(std::move(r)), action(std::move(a)), timeout(std::move(t)) { startTimer(25); }
        void timerCallback() override
        {
            if (ready()) { stopTimer(); auto next=std::move(action);delete this;next(); }
            else if (--remaining<=0) { stopTimer();auto fail=std::move(timeout);delete this;fail(); }
        }
    };
    const auto await = [check, finish](std::function<bool()> ready, std::function<void()> action)
    {
        new Await(std::move(ready),std::move(action),[check,finish]{check("modal_callback_completed",false);finish();});
    };
    const auto read = [](const juce::File& file)
    {
        juce::WavAudioFormat wav;
        return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(file.createInputStream().release(),true));
    };
    const auto samples = [](juce::AudioFormatReader& reader)
    {
        juce::AudioBuffer<float> data(static_cast<int>(reader.numChannels), static_cast<int>(reader.lengthInSamples));
        reader.read(&data,0,data.getNumSamples(),0,true,true);return data;
    };
    const auto source = folder.getChildFile("source-stereo.wav");
    {
        juce::AudioBuffer<float> data(2,48000);
        for(int i=0;i<48000;++i)
        {
            data.setSample(0,i,0.2f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*440*i/48000)));
            data.setSample(1,i,0.3f*static_cast<float>(std::sin(juce::MathConstants<double>::twoPi*880*i/48000)));
        }
        auto stream=source.createOutputStream();stream->setPosition(0);stream->truncate();
        juce::WavAudioFormat wav;
        auto writer=std::unique_ptr<juce::AudioFormatWriter>(wav.createWriterFor(stream.release(),48000,2,24,{},0));
        check("fixture_stereo_tones_written",writer && writer->writeFromAudioSampleBuffer(data,0,data.getNumSamples()));
    }
    ProjectModel model; (void)model.addAudioFile(source,1.0);auto data=model.snapshot();
    data.tracks.front().compose=false;
    audio.syncProject(data);audio.setPosition(0.37);
    const auto deviceRate=audio.playbackSampleRate();const auto previousPosition=audio.position();
    const auto trackId=data.tracks.front().id;
    juce::String error;
    for(const auto rate : {8000,44100,48000,96000,192000})
        for(const auto depth : {16,24,32})
        {
            const auto suffix=juce::String(rate)+"-"+juce::String(depth);
            const auto stereo=folder.getChildFile("stereo-"+suffix+".wav");
            const auto mono=folder.getChildFile("mono-"+suffix+".wav");
            check("write_stereo_"+suffix,audio.exportWav(stereo,error,{},0,0,{rate,2,depth}));
            check("write_mono_"+suffix,audio.exportWav(mono,error,{},0,0,{rate,1,depth}));
            auto sr=read(stereo),mr=read(mono);
            const auto headers=sr && mr && sr->sampleRate==rate && mr->sampleRate==rate
                && sr->bitsPerSample==depth && mr->bitsPerSample==depth
                && sr->numChannels==2 && mr->numChannels==1
                && sr->usesFloatingPointData==(depth==32) && mr->usesFloatingPointData==(depth==32)
                && sr->lengthInSamples==rate && mr->lengthInSamples==rate;
            check("header_duration_"+suffix,headers);
            if(!headers) continue;
            const auto stereoData=samples(*sr),monoData=samples(*mr);
            float difference=0;
            for(int i=0;i<rate;++i)difference=std::max(difference,std::abs(monoData.getSample(0,i)
                -0.5f*(stereoData.getSample(0,i)+stereoData.getSample(1,i))));
            check("both_channels_averaged_"+suffix,difference<(depth==16?0.00004f:0.000001f));
            int crossings=0;
            for(int i=rate/5+1;i<rate*4/5;++i)
                if(stereoData.getSample(1,i-1)<=0 && stereoData.getSample(1,i)>0)++crossings;
            check("pitch_preserved_"+suffix,std::abs(crossings/0.6-880)<3);
            check("device_transport_restored_"+suffix,audio.playbackSampleRate()==deviceRate
                && std::abs(audio.position()-previousPosition)<1e-7 && !audio.isPlaying());
        }
    const auto legacy=folder.getChildFile("legacy-default.wav");
    check("old_export_call_compatible",audio.exportWav(legacy,error));
    auto legacyReader=read(legacy);
    check("legacy_defaults_stereo24_device_rate",legacyReader && legacyReader->numChannels==2
        && legacyReader->bitsPerSample==24 && legacyReader->sampleRate==deviceRate);
    legacyReader.reset();
    const auto rangeFile=folder.getChildFile("range.wav");
    check("range_export",audio.exportWav(rangeFile,error,trackId,0.25,0.75,{44100,1,32}));
    auto rangeReader=read(rangeFile);
    check("range_half_second_at_selected_rate",rangeReader && rangeReader->lengthInSamples==22050 && rangeReader->sampleRate==44100);
    rangeReader.reset();
    data.tracks.front().pan=1;audio.syncProject(data);
    check("right_panned_export",audio.exportWav(folder.getChildFile("right-mono.wav"),error,trackId,0,0,{48000,1,32}));
    auto right=read(folder.getChildFile("right-mono.wav"));
    check("mono_retains_right_panned_audio",right && samples(*right).getRMSLevel(0,4800,30000)>0.08f);
    const auto protectedFile=folder.getChildFile("protected.wav");protectedFile.replaceWithText("existing audio must survive");
    for(const auto options : {WavExportOptions{7999,2,24},WavExportOptions{48000,3,24},WavExportOptions{48000,2,20}})
        check("invalid_settings_preserve_existing_file",!audio.exportWav(protectedFile,error,{},0,0,options)
            && error.isNotEmpty() && protectedFile.loadFileAsString()=="existing audio must survive");
    const auto blocked=folder.getChildFile("blocked.wav");blocked.createDirectory();blocked.getChildFile("keep.txt").replaceWithText("keep");
    check("failed_replace_preserves_target_and_transport",!audio.exportWav(blocked,error,{},0,0,{96000,1,16})
        && blocked.getChildFile("keep.txt").loadFileAsString()=="keep"
        && audio.playbackSampleRate()==deviceRate && std::abs(audio.position()-previousPosition)<1e-7);
    check("successful_overwrite",audio.exportWav(protectedFile,error,{},0,0,{48000,2,16}));
    auto overwritten=read(protectedFile);check("overwritten_wav_readable",overwritten && overwritten->bitsPerSample==16);

    componentExportChecks(folder,audio,check);

    // Exercise confirmed options through the shared renderer without a native file picker.
    data.tracks.front().pan=0;project.replace(data);project.dispatchPendingMessages();stopTimer();
    preferences->removeValue("export.wav.channels");preferences->removeValue("export.wav.bitDepth");preferences->removeValue("export.wav.sampleRate");
    const auto uiFile=folder.getChildFile("ui-track.wav");
    showWavExportOptions([this,trackId,uiFile](WavExportOptions options){startExport({{trackId,"test",uiFile}},{},{},options);});
    auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
    check("export_opens_settings_before_render",dialog && !exportWaitingForRender && pendingExport.empty());
    if(!dialog){finish();return;}
    auto* advanced=dialog->getCustomComponent(0);
    auto* expand=advanced ? dynamic_cast<juce::Button*>(advanced->findChildWithID("export-advanced")) : nullptr;
    auto* breath=advanced ? dynamic_cast<juce::ToggleButton*>(advanced->findChildWithID("export-breath")) : nullptr;
    auto* nonBreath=advanced ? dynamic_cast<juce::ToggleButton*>(advanced->findChildWithID("export-non-breath")) : nullptr;
    check("advanced_initially_collapsed",expand && breath && nonBreath && !breath->isVisible() && !nonBreath->isVisible());
    if(expand && breath && nonBreath)
    {
        expand->onClick();
        check("unsupported_choices_visible_but_disabled",breath->isVisible() && nonBreath->isVisible() && !breath->isEnabled() && !nonBreath->isEnabled());
        if(auto stream=folder.getChildFile("advanced-unsupported.png").createOutputStream())
            juce::PNGImageFormat().writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*stream);
        expand->onClick();
    }
    check("default_controls",dialog->getComboBoxComponent("channels")->getSelectedId()==2
        && dialog->getComboBoxComponent("depth")->getSelectedId()==2 && dialog->getComboBoxComponent("rate")->getSelectedId()==1);
    dialog->getComboBoxComponent("channels")->setSelectedId(1,juce::dontSendNotification);
    dialog->getComboBoxComponent("depth")->setSelectedId(3,juce::dontSendNotification);
    dialog->getComboBoxComponent("rate")->setSelectedId(96000,juce::dontSendNotification);
    if(auto stream=folder.getChildFile("wav-export-settings.png").createOutputStream())
    {
        stream->setPosition(0);stream->truncate();
        juce::PNGImageFormat().writeImageToStream(dialog->createComponentSnapshot(dialog->getLocalBounds()),*stream);
    }
    dialog->exitModalState(1);
    await([this]{return exportWaitingForRender;},[this,folder,check,finish,read,uiFile,await]
    {
        check("confirmed_options_frozen_before_render",exportWaitingForRender && pendingExportOptions.channels==1
            && pendingExportOptions.bitDepth==32 && pendingExportOptions.sampleRate==96000);
        check("preferences_saved_to_disk",preferences->getIntValue("export.wav.channels")==1
            && preferences->getIntValue("export.wav.sampleRate")==96000 && !preferences->needsToBeSaved());
        preferences->setValue("export.wav.sampleRate",44100);
        finishExport();auto reader=read(uiFile);
        check("track_export_uses_frozen_settings",reader && reader->numChannels==1 && reader->bitsPerSample==32 && reader->sampleRate==96000);
        const auto lastFile=folder.getChildFile("ui-last-render.wav");
        showWavExportOptions([this,lastFile](WavExportOptions options){startExport({{{},{},lastFile}},{"marquee-note"},{0.25,0.75},options);});
        auto* next=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
        check("range_export_remembers_controls",next && next->getComboBoxComponent("channels")->getSelectedId()==1
            && next->getComboBoxComponent("depth")->getSelectedId()==3 && next->getComboBoxComponent("rate")->getSelectedId()==44100);
        if(!next){finish();return;}
        next->getComboBoxComponent("channels")->setSelectedId(2,juce::dontSendNotification);
        next->getComboBoxComponent("depth")->setSelectedId(1,juce::dontSendNotification);
        next->exitModalState(1);
        await([this]{return exportWaitingForRender;},[this,folder,check,finish,read,lastFile,await]
        {
            finishExport();auto reader=read(lastFile);
            check("last_render_range_uses_settings",reader && reader->numChannels==2 && reader->bitsPerSample==16
                && reader->sampleRate==44100 && reader->lengthInSamples==22050);
            const auto cancelledFile=folder.getChildFile("cancelled.wav");cancelledFile.replaceWithText("keep");
            const auto revision=project.revisionNumber();
            showWavExportOptions([this,cancelledFile](WavExportOptions options){startExport({{{},{},cancelledFile}},{},{},options);});
            auto* cancel=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
            if(!cancel){check("cancel_dialog_exists",false);finish();return;}
            cancel->getComboBoxComponent("rate")->setSelectedId(192000,juce::dontSendNotification);
            juce::Component::SafePointer<juce::AlertWindow> cancelledDialog(cancel);
            cancel->exitModalState(0);
            await([cancelledDialog]{return cancelledDialog==nullptr;},[this,folder,check,finish,cancelledFile,revision,await]
            {
                check("cancel_no_render_write_or_project_edit",!exportWaitingForRender && pendingExport.empty()
                    && cancelledFile.loadFileAsString()=="keep" && project.revisionNumber()==revision);
                check("cancel_keeps_last_confirmed_settings",preferences->getIntValue("export.wav.sampleRate")==44100);
                requestTrackExport(project.snapshot().tracks.front().id);
                auto* settings=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                check("track_choice_opens_format_before_file_picker",settings && chooser==nullptr && !exportWaitingForRender && pendingExport.empty());
                if(!settings){finish();return;}
                check("repeated_export_still_prompts_with_previous_format",settings->getComboBoxComponent("channels")->getSelectedId()==2
                    && settings->getComboBoxComponent("depth")->getSelectedId()==1 && settings->getComboBoxComponent("rate")->getSelectedId()==44100);
                if(auto stream=folder.getChildFile("track-wav-settings.png").createOutputStream())
                    juce::PNGImageFormat().writeImageToStream(settings->createComponentSnapshot(settings->getLocalBounds()),*stream);
                juce::Component::SafePointer<juce::AlertWindow> settingsDialog(settings);
                settings->exitModalState(0);
                await([settingsDialog]{return settingsDialog==nullptr;},[this,folder,check,finish,revision,await] {
                    check("format_cancel_never_opens_file_picker",chooser==nullptr && !exportWaitingForRender && project.revisionNumber()==revision);
                    requestTrackExport({});
                    auto* all=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                    check("all_tracks_choice_also_opens_format_first",all && chooser==nullptr && !exportWaitingForRender);
                    if(!all){finish();return;}
                    juce::Component::SafePointer<juce::AlertWindow> allDialog(all);all->exitModalState(0);
                    await([allDialog]{return allDialog==nullptr;},[this,folder,check,finish,await] {
                        lastRenderedNoteIds={"marquee-note"};lastRenderedSpan={0.25,0.75};exportLastRender();
                        auto* last=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                        check("last_render_export_also_opens_format_first",last && chooser==nullptr && !exportWaitingForRender);
                        if(!last){finish();return;}
                        juce::Component::SafePointer<juce::AlertWindow> lastDialog(last);last->exitModalState(0);
                        await([lastDialog]{return lastDialog==nullptr;},[this,folder,check,finish,await]{
                            check("all_entry_cancellations_leave_no_pending_export",chooser==nullptr && pendingExport.empty() && !exportWaitingForRender);
                            auto data=project.snapshot();auto& track=data.tracks.front();track.compose=true;track.pitchAlgorithm=PitchAlgorithm::world;
                            NoteData note;note.id="advanced-note";note.label="a";note.startSeconds=0;note.durationSeconds=1;note.midiNote=60;
                            track.clips.front().notes={note};
                            const auto voiceTrackId=track.id;
                            auto inactive=track;inactive.id="inactive-material";inactive.referenceOnly=true;inactive.compose=false;
                            auto empty=track;empty.id="muted-clips";empty.clips.front().muted=true;
                            auto scopeTest=data;scopeTest.tracks.push_back(inactive);scopeTest.tracks.push_back(empty);
                            auto scopeTargets=exportTargets(scopeTest,folder.getChildFile("scope.wav"),{},"untitled");
                            check("component_scope_ignores_inactive_material_and_muted_clips",
                                AudioEngine::componentExportIssue(projectForExport(scopeTest,scopeTargets,{}),audio.currentUtauResamplerFile()).isEmpty());
                            auto accompaniment=track;accompaniment.id="advanced-accompaniment";accompaniment.compose=false;
                            data.tracks.push_back(accompaniment);project.replace(data);project.dispatchPendingMessages();stopTimer();
                            auto selected=std::make_shared<std::optional<WavExportOptions>>();
                            const auto advancedFile=folder.getChildFile("ui-breath-single-track.wav");
                            showWavExportOptions([this,selected,advancedFile,voiceTrackId](WavExportOptions o){
                                *selected=o;startExport({{voiceTrackId,"voice",advancedFile}},{},{},o);
                            },voiceTrackId);
                            auto* supported=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                            check("supported_advanced_dialog_exists",supported!=nullptr);
                            if(!supported){finish();return;}
                            auto* panel=supported->getCustomComponent(0);
                            auto* header=dynamic_cast<juce::Button*>(panel->findChildWithID("export-advanced"));
                            auto* breath=dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("export-breath"));
                            auto* harmonic=dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("export-non-breath"));
                            header->onClick();
                            check("supported_choices_enabled_and_default_full",breath->isEnabled() && harmonic->isEnabled() && !breath->getToggleState() && !harmonic->getToggleState());
                            breath->setToggleState(true,juce::dontSendNotification);breath->onClick();
                            check("select_breath_only",breath->getToggleState() && !harmonic->getToggleState());
                            harmonic->setToggleState(true,juce::dontSendNotification);harmonic->onClick();
                            check("advanced_choices_mutually_exclusive",!breath->getToggleState() && harmonic->getToggleState());
                            harmonic->setToggleState(false,juce::dontSendNotification);harmonic->onClick();
                            check("advanced_can_return_to_full",!breath->getToggleState() && !harmonic->getToggleState());
                            breath->setToggleState(true,juce::dontSendNotification);breath->onClick();
                            if(auto stream=folder.getChildFile("advanced-supported.png").createOutputStream())
                                juce::PNGImageFormat().writeImageToStream(supported->createComponentSnapshot(supported->getLocalBounds()),*stream);
                            header->onClick();
                            check("collapsed_retains_visible_selection_summary",header->getButtonText().contains(breath->getButtonText()));
                            supported->exitModalState(1);
                            await([this,selected]{return selected->has_value() && !audio.renderProgress();},[this,check,finish,await,selected,advancedFile]{
                                check("advanced_selection_passed_to_export",selected->value().component==WavExportComponent::breath);
                                check("ui_prepares_component_for_single_track",exportWaitingForRender && audio.activeRenderBackends().contains("+breath"));
                                const auto revision=project.revisionNumber();finishExport();
                                check("ui_component_export_writes_and_restores",advancedFile.getSize()>44 && !exportWaitingForRender
                                    && pendingExportOptions.component==WavExportComponent::full && project.revisionNumber()==revision);
                                showWavExportOptions([](WavExportOptions){});
                                auto* again=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());
                                if(!again){check("next_export_dialog_exists",false);finish();return;}
                                auto* panel=again->getCustomComponent(0);
                                auto* breath=dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("export-breath"));
                                auto* harmonic=dynamic_cast<juce::ToggleButton*>(panel->findChildWithID("export-non-breath"));
                                check("new_export_defaults_to_full_voice",!breath->getToggleState() && !harmonic->getToggleState() && !breath->isVisible());
                                juce::Component::SafePointer<juce::AlertWindow> ref(again);again->exitModalState(0);
                                await([ref]{return ref==nullptr;},finish);
                            });
                        });
                    });
                });
            });
        });
    });
}
}
