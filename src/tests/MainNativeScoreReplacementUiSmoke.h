#pragma once
// Included ONLY by MainComponent.cpp after its MaterialInputSession definition.
#include "../MaterialPronunciationCatalog.h"
#include "../PronunciationCandidateBox.h"
#include "../NativeNoteTiming.h"
#include "../NativeSourceEdgePlan.h"
#include "../backend/AnalysisService.h"
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/UstExchange.h"
#include "ForcedPortamentoAudioSmoke.h"
#include <juce_audio_formats/juce_audio_formats.h>
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
bool MainComponent::diagnosticNativeScoreReplacementUi(const juce::File& output,const juce::File& nsfModel)
{
    const auto root=output.getChildFile("native-score-picker-ui-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto finish=[&]{dismissMaterialPronunciationInput();std::cout<<"native_score_replacement_ui="<<ok<<"|fixture="<<root.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return root.getChildFile("verification.txt").replaceWithText(log)&&ok;};
#if !JUCE_WINDOWS
    check("actual_Windows_UI_message_pump",false);return finish();
#else
    check("actual_Main_message_thread",juce::MessageManager::getInstance()->isThisTheMessageThread());
    const auto isolated=juce::SystemStats::getEnvironmentVariable("HACHI_TEST_SETTINGS_DIR",{});check("isolated_test_preferences",preferences&&isolated.isNotEmpty()&&preferences->getFile().getFullPathName().contains(isolated));
    check("real_native_NSF_model",backend::NsfHifiganRenderer::modelAvailable(nsfModel));if(!ok)return finish();
    audio.stop();stopTimer();pianoRoll.setReadsVoicebankInBackground(false);pianoRoll.diagnosticSetCulling(false);
    preferences->setValue("algorithm.inference",2);preferences->setValue("algorithm.device",1);preferences->setValue("algorithm.gameModel","medium");
    auto config=backend::AnalysisService::configFromProperties(preferences.get());auto status=backend::AnalysisService::status(config);
    for(const auto& location:{nsfModel.getParentDirectory(),nsfModel,nsfModel.getParentDirectory().getParentDirectory().getChildFile("models")})
    {
        if(!status.gameModelReady&&location.getChildFile("game").isDirectory())preferences->setValue("algorithm.gamePath",location.getChildFile("game").getFullPathName());
        if(!status.fcpeModelReady&&location.getChildFile("fcpe/fcpe.onnx").existsAsFile())preferences->setValue("algorithm.fcpePath",location.getChildFile("fcpe/fcpe.onnx").getFullPathName());
        config=backend::AnalysisService::configFromProperties(preferences.get());status=backend::AnalysisService::status(config);
    }
    check("real_GAME_FCPE_resources_no_fake_analysis",status.gameModelReady&&status.fcpeModelReady&&status.onnxRuntimeReady);if(!ok)return finish();
    setSize(1440,980);resized();diagnosticPressTool(PianoRollComponent::Tool::note);
    struct Window:juce::DocumentWindow{Window():DocumentWindow("Native UST material picker",Palette::panel,DocumentWindow::allButtons){setUsingNativeTitleBar(true);}void closeButtonPressed()override{setVisible(false);}};
    std::unique_ptr<Window> host;if(!isShowing()){host=std::make_unique<Window>();host->setContentNonOwned(this,false);host->centreWithSize(1440,980);host->setVisible(true);}
    const auto pump=[&]{MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i){if(message.message==WM_QUIT){PostQuitMessage(int(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}project.dispatchPendingMessages();pianoRoll.diagnosticRefresh();return true;};
    const auto wait=[&](const std::function<bool()>& ready,int timeout=30000){const auto deadline=juce::Time::getMillisecondCounterHiRes()+timeout;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<deadline){if(!pump())return false;juce::Thread::sleep(3);}pump();return ready();};
    const auto closeWarnings=[&]{for(int i=0;i<8;++i){auto* modal=juce::Component::getCurrentlyModalComponent();if(!modal)break;modal->exitModalState(0);pump();}};
    const auto bank=root.getChildFile("material");bank.createDirectory();const auto phrase=bank.getChildFile("da_re_mo.wav"),gaFile=bank.getChildFile("ga.wav");
    const auto makeWave=[&](const juce::File& file,int words)
    {
        constexpr int rate=48000;juce::AudioBuffer<float> pcm(1,words*24000);std::uint32_t random=0x124899a3;
        for(int i=0;i<pcm.getNumSamples();++i)
        {
            const auto local=(i%24000)/double(rate);const auto time=i/double(rate);random=1664525u*random+1013904223u;
            const auto noise=(float((random>>8)&0xffffff)/float(0xffffff)*2-1)*.12f;
            const auto phase=juce::MathConstants<double>::twoPi*261.625565*time;
            const auto vowel=float(.13*std::sin(phase)+.025*std::sin(2*phase)+.012*std::sin(3*phase));
            pcm.setSample(0,i,local<.1?noise:vowel);
        }
        auto stream=file.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),rate,1,24,{},0):nullptr);
        return writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples());
    };
    check("physical_full_consonant_and_vowel_fixtures",makeWave(phrase,3)&&makeWave(gaFile,1));
    std::vector<SampleRegionSetting> phraseRows;
    for(int i=0;i<3;++i){SampleRegionSetting row;row.name=i==0?"da":i==1?"re":"mo";row.materialAudioId="phrase-variant";row.materialRegionId=row.name;row.regionStartSeconds=.5*i;row.regionEndSeconds=.5*(i+1);row.alignmentSeconds=row.regionStartSeconds+.1;row.fixedDurationSeconds=.16;row.overlapSeconds=.015;row.role=NativeSegmentRole::vowel;row.provenance="explicit-generated-source-fixture";if(i<2)row.connectedNextRegionId=i==0?"re":"mo";NativePronunciationEdges edge;edge.entryFromSourceSeconds=row.regionStartSeconds;edge.entryToSourceSeconds=row.regionStartSeconds+.015;edge.exitFromSourceSeconds=row.regionEndSeconds-.025;edge.exitToSourceSeconds=edge.terminationSourceSeconds=row.regionEndSeconds;edge.provenance="explicit-test-source-edges";row.nativePronunciationEdges=edge;phraseRows.push_back(row);}
    auto ga=phraseRows.front();ga.name="ga";ga.materialAudioId="ga-variant";ga.materialRegionId="ga";ga.connectedNextRegionId.clear();OttoMaterialIO::Document document;document.audio={{"phrase-variant",phrase.getFileName(),phraseRows},{"ga-variant",gaFile.getFileName(),{ga}}};
    const auto manifest=bank.getChildFile("otto.ini");check("authoritative_registered_pronunciations",OttoMaterialIO::saveDocument(manifest,document,error));
    const auto phraseHash=juce::SHA256(phrase).toHexString(),gaHash=juce::SHA256(gaFile).toHexString(),ottoHash=juce::SHA256(manifest).toHexString();
    const auto ust=root.getChildFile("score.ust"),ustx=root.getChildFile("score.ustx");
    const juce::String ustText="[#VERSION]\nUST Version1.20\n[#SETTING]\nTempo=120\nTracks=1\nMode2=True\n[#0000]\nLength=480\nLyric=da\nNoteNum=60\nPreUtterance=120\nVoiceOverlap=35\nVelocity=110\nIntensity=80\n[#0001]\nLength=480\nLyric=re\nNoteNum=62\nVelocity=130\nIntensity=85\n[#0002]\nLength=480\nLyric=ga\nNoteNum=64\nPreUtterance=100\nVoiceOverlap=-10\nVelocity=100\nIntensity=90\n[#0003]\nLength=480\nLyric=keep\nNoteNum=65\n[#TRACKEND]\n";
    const juce::String ustxText="ustx_version: '0.8'\nresolution: 480\nname: Native picker fixture\ntempos: [{position: 0, bpm: 120}]\ntracks: [{track_name: Hachi fixture}]\nvoice_parts:\n- track_no: 0\n  position: 0\n  duration: 1920\n  notes:\n  - {position: 0, duration: 480, tone: 60, lyric: da}\n  - {position: 480, duration: 480, tone: 62, lyric: re}\n  - {position: 960, duration: 480, tone: 64, lyric: ga}\n  - {position: 1440, duration: 480, tone: 65, lyric: keep}\n";
    check("actual_UST_USTX_format_fixtures",ust.replaceWithText(ustText)&&ustx.replaceWithText(ustxText));if(!ok)return finish();
    const auto picker=[&]()->PronunciationCandidateBox*{return materialInputSession?materialInputSession->picker.getComponent():nullptr;};
    const auto choose=[&](const juce::File& file,const juce::StringArray& regions)
    {
        if(!wait([&]{return materialInputSession&&materialInputSession->ready&&!materialInputSession->resolving&&picker();}))return false;
        auto* box=picker();for(int i=0;i<box->diagnosticCandidateCount();++i)if(const auto candidate=box->diagnosticCandidate(i))
        {const auto ids=candidate->regionIds.isEmpty()?juce::StringArray{candidate->regionId}:candidate->regionIds;if(candidate->audio==file&&ids==regions){box->diagnosticSelect(i);box->diagnosticCommit();return true;}}return false;
    };
    const auto pointFor=[&](const NoteData& note,double clipStart){return juce::Point<float>(float(pianoRoll.pixelForSeconds(clipStart+note.startSeconds+note.durationSeconds*.5)),pianoRoll.diagnosticYForMidi(note.midiNote));};
    const auto eventAt=[&](juce::Point<float> point,int clicks,int modifiers){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(modifiers),0,0,0,0,0,&pianoRoll,&pianoRoll,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),clicks,false);};
    const auto noteHash=[](NoteData note){note.clipPartId.clear();ProjectData data;TrackData track;track.id="track";ClipData clip;clip.id="clip";clip.durationSeconds=4;clip.notes={std::move(note)};track.clips={clip};data.tracks={track};ProjectModel model;model.resetDocument(data);return model.contentFingerprint();};
    for(const auto& file:{ust,ustx})
    {
        dismissMaterialPronunciationInput();importUstFile(file,ProjectModel::UstImportMode::replaceProject,65001,TrackType::hachi);pump();closeWarnings();
        auto imported=project.snapshot();check(file==ust?"real_Main_UST_Hachi_import":"real_Main_USTX_Hachi_import",imported.tracks.size()==1&&imported.tracks[0].trackType==TrackType::hachi&&imported.tracks[0].clips.size()==1&&imported.tracks[0].clips[0].notes.size()==4);if(imported.tracks.empty()||imported.tracks[0].clips.empty()||imported.tracks[0].clips[0].notes.size()!=4)return finish();
        setAssetManagerVisible(true);check("actual_Manager_root_registration",assetManager&&assetManager->registerMaterialRoot(bank,error));selectedTrackId=imported.tracks[0].id;selectedClipId=imported.tracks[0].clips[0].id;pianoRoll.setFocusedTrack(selectedTrackId);pianoRoll.setFocusedClip(selectedClipId);pianoRoll.diagnosticRefresh();
        const auto& old=imported.tracks[0].clips[0].notes;std::vector<juce::String> ids{old[0].id,old[1].id,old[2].id};const auto before=project.contentFingerprint();const auto revision=project.revisionNumber();
        const auto selectCtrl=[&]{pianoRoll.setSelectedNoteIds({});for(int i=0;i<3;++i){const auto event=eventAt(pointFor(old[std::size_t(i)],imported.tracks[0].clips[0].startSeconds),1,juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::ctrlModifier);pianoRoll.mouseDown(event);pianoRoll.mouseUp(event);pump();}};
        selectCtrl();check("actual_Ctrl_multiselect_three_notes",pianoRoll.diagnosticSelectedCount()==3);
        const auto openReclick=[&]{const auto event=eventAt(pointFor(old[0],imported.tracks[0].clips[0].startSeconds),1,juce::ModifierKeys::leftButtonModifier);pianoRoll.mouseDown(event);pianoRoll.mouseUp(event);return wait([&]{return materialInputSession&&materialInputSession->ready&&picker();});};
        check("selected_score_reclick_opens_existing_picker",openReclick());check("reclick_keeps_Ctrl_selection_and_lyric_query",pianoRoll.diagnosticSelectedCount()==3&&picker()&&picker()->queryEditor().getText()=="da");
        if(file==ust&&picker())
        {
            sourcePreviewOfflineForTest=true;audio.devices().closeAudioDevice();audio.prepareToPlay(257,48000);audio.setPosition(.25);
            auto* box=picker();juce::Button* preview=nullptr;bool chain=false;
            for(int i=0;i<box->diagnosticCandidateCount();++i)if(const auto c=box->diagnosticCandidate(i);c&&c->audio==phrase&&c->regionIds==juce::StringArray{"da","re"})
            {box->diagnosticSelect(i);chain=true;break;}
            for(int i=0;i<box->getNumChildComponents();++i)if(box->getChildComponent(i)->getComponentID()=="material.candidate.preview")preview=dynamic_cast<juce::Button*>(box->getChildComponent(i));
            if(preview&&preview->isEnabled())preview->triggerClick();
            check("actual_chain_preview_button_uses_shared_raw_transport",chain&&preview&&wait([&]{return sourcePreviewActive&&sourcePreviewOwner==box;}));
            juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> sourceReader(formats.createReaderFor(phrase));
            juce::AudioBuffer<float> reference(1,48000);reference.clear();bool exact=sourceReader&&sourcePreviewActive;
            if(sourceReader)sourceReader->read(&reference,0,48000,0,true,false);
            int read=0;while(audio.isPlaying()&&read<50000)
            {
                juce::AudioBuffer<float> block(2,257);block.clear();audio.getNextAudioBlock({&block,0,257});
                for(int i=0;i<257;++i){const auto expected=read+i<48000?reference.getSample(0,read+i):0.f;exact&=std::abs(block.getSample(0,i)-expected)<1.e-6f&&std::abs(block.getSample(1,i)-expected)<1.e-6f;}
                read+=257;
            }
            check("chain_preview_exact_da_re_PCM_and_EOF_does_not_include_mo",exact&&!audio.isPlaying()&&std::abs(audio.position()-1.0)<1.e-7&&read>=48000);
            stopSourcePreview(box);sourcePreviewOfflineForTest=false;
            check("preview_restores_song_without_edit_or_resume",!sourcePreviewActive&&!audio.isPlaying()&&std::abs(audio.position()-.25)<1.e-7&&project.contentFingerprint()==before&&project.revisionNumber()==revision);
        }
        check("explicit_da_re_subsequence_choice_not_mo",choose(phrase,{"da","re"}));check("remaining_ga_prompt_no_partial_commit",wait([&]{return picker()&&materialInputSession&&!materialInputSession->resolving&&picker()->queryEditor().getText()=="ga";})&&project.contentFingerprint()==before);
        if(picker())picker()->keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));check("Esc_cancels_entire_staged_sequence",wait([&]{return !materialInputSession;})&&project.contentFingerprint()==before&&project.revisionNumber()==revision);
        selectCtrl();check("reopen_actual_selected_reclick",openReclick());check("choose_chain_then_keep_ga_context",choose(phrase,{"da","re"})&&wait([&]{return picker()&&materialInputSession&&!materialInputSession->resolving&&picker()->queryEditor().getText()=="ga";}));check("explicit_independent_ga_source_choice",choose(gaFile,{"ga"}));
        check("single_atomic_commit_after_last_choice",wait([&]{return !materialInputSession;})&&project.revisionNumber()==revision+1);check("real_source_analysis_finished",wait([&]{return pendingNativeAnalyses==0;},180000));log+="analysis_backend="+lastNativeAnalysisBackend+"\n"+lastNativeAnalysisTrace+"\n";
        const auto assigned=project.snapshot();const auto& parent=assigned.tracks[0].clips[0];bool music=true,flags=true,noMo=true;
        for(std::size_t i=0;i<4;++i){const auto& n=parent.notes[i];music&=n.id==old[i].id&&n.label==old[i].label&&n.startSeconds==old[i].startSeconds&&n.durationSeconds==old[i].durationSeconds&&n.midiNote==old[i].midiNote&&juce::JSON::toString(backend::ustexchange::noteState(n,assigned,parent.startSeconds,true))==juce::JSON::toString(backend::ustexchange::noteState(old[i],imported,imported.tracks[0].clips[0].startSeconds,true));if(i<3)flags&=n.nativeScoreMaterialAligned;noMo&=n.materialRegionId!="mo";}
        check("all_score_ids_time_duration_MIDI_lyrics_controls_unchanged",music);check("only_requested_source_rows_aligned_no_mo",flags&&noMo&&!parent.notes[3].nativeScoreMaterialAligned);check("unselected_note_unchanged_except_explicit_container_owner",noteHash(parent.notes[3])==noteHash(old[3]));
        check("da_re_share_one_clock_ga_separate",parent.notes[0].clipPartId==parent.notes[1].clipPartId&&parent.notes[0].clipPartId!=parent.notes[2].clipPartId);
        bool fullPitch=true;for(int i=0;i<3;++i){const auto& note=parent.notes[std::size_t(i)];fullPitch&=note.sourcePitchMeasured&&note.sourceMidiCenter>=0&&std::any_of(note.contour.begin(),note.contour.end(),[](const auto& p){return p.voiced;});}
        check("registered_rows_keep_real_full_file_FCPE_pitch_independent_of_GAME_mask",fullPitch);
        for(int i=0;i<3;++i){const auto& n=parent.notes[std::size_t(i)];const auto owner=std::find_if(parent.parts.begin(),parent.parts.end(),[&](const auto& part){return part.id==n.clipPartId;});bool support=false;if(owner!=parent.parts.end()){auto local=n;local.startSeconds-=owner->startSeconds;NativeScoreMaterialSupport span;support=nativeScoreMaterialAudioSupport(local,*owner,nativeClipClock(*owner),span,error)&&span.targetBegin<local.startSeconds-.001&&std::abs(nativeTargetTimeAt(nativeClipClock(*owner),span.alignmentSource-owner->sourceOffsetSeconds)-local.startSeconds)<1.e-7;}check("full_source_consonant_support_before_unchanged_score_P",support);}
        const auto saved=root.getChildFile(file.getFileName()+".hjpx");ProjectModel cold;check("HJPX_cold_aligned_support_roundtrip",project.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().tracks[0].clips[0].notes[0].nativeScoreMaterialAligned);
        if(file==ust)
        {
            auto renderData=cold.snapshot();auto& renderTrack=renderData.tracks[0];renderTrack.pitchAlgorithm=PitchAlgorithm::nsfHifigan;renderTrack.nativeNsfAudio=true;renderTrack.normalizeVolume=false;renderTrack.nsfSmoothPitchTransitions=false;
            double durationEquivalent=0;
            for(const auto& track:renderData.tracks)for(const auto& clip:track.clips)
            {
                log+="cold_top_clip="+clip.id+" start="+juce::String(clip.startSeconds,12)+" duration="+juce::String(clip.durationSeconds,12)+" muted="+juce::String(int(clip.muted))+" analysisPending="+juce::String(int(clip.materialSourceAnalysisPending))+" sourcePitchPending="+juce::String(int(clip.nativeSourcePitchPending))+"\n";
                if(!clip.muted)durationEquivalent=std::max(durationEquivalent,clip.startSeconds+clip.durationSeconds);
                for(const auto& part:clip.parts)log+="cold_child="+part.id+" start="+juce::String(part.startSeconds,12)+" duration="+juce::String(part.durationSeconds,12)+" muted="+juce::String(int(part.muted))+" analysisPending="+juce::String(int(part.materialSourceAnalysisPending))+" source="+part.sourceFile.getFileName()+"\n";
            }
            log+="projectDuration_equivalent="+juce::String(durationEquivalent,12)+"\n";
            AudioEngine offline(false);offline.setHifiganModelDirectory(nsfModel);offline.setInferenceConfiguration(backend::InferenceBackend::cpu,-1);offline.prepareToPlay(251,48000);offline.syncProject(renderData);
            check("actual_native_render_completed",wait([&]{const auto progress=offline.renderProgress();return !progress||*progress>=.999;},180000));log+="native_backend="+offline.activeRenderBackends()+"\nwarning="+offline.activeRenderWarnings()+"\n";const auto progress=offline.renderProgress();log+="offline_render_progress="+(progress?juce::String(*progress,9):juce::String("none"))+"\n";const auto start=offline.nativeScorePrerollStart();check("score_zero_has_typed_negative_audio_support_not_music_shift",start<-.001&&renderData.tracks[0].clips[0].notes[0].startSeconds==0);
            const auto rendered=root.getChildFile("native-full-consonant.wav");check("native_export_includes_negative_preroll",offline.activeRenderWarnings().isEmpty()&&offline.exportWav(rendered,error));juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(rendered));juce::AudioBuffer<float> decoded;if(reader){decoded.setSize(1,int(reader->lengthInSamples));reader->read(&decoded,0,decoded.getNumSamples(),0,true,false);}const auto rate=reader?reader->sampleRate:0.0;
            log+="exported_frames="+juce::String(decoded.getNumSamples())+" sampleRate="+juce::String(rate,3)+" seconds="+juce::String(rate>0?decoded.getNumSamples()/rate:0.0,12)+" prerollOrigin="+juce::String(start,12)+" expected_full_seconds="+juce::String(durationEquivalent-start,12)+"\n";
            const auto rms=[&](double from,double to){if(rate<=0)return 0.f;const int first=juce::jlimit(0,decoded.getNumSamples(),juce::roundToInt(from*rate)),last=juce::jlimit(first,decoded.getNumSamples(),juce::roundToInt(to*rate));return last>first?decoded.getRMSLevel(0,first,last-first):0.f;};
            check("actual_positive_early_and_late_consonant_PCM_before_score_zero",reader&&rms(-start*.2,-start*.4)>1.e-5f&&rms(-start*.65,-start*.9)>1.e-5f);
            const auto rePitch=forcedPortamentoAudioSmokeDetail::measurePitch(decoded,rate,-start+.75),gaPitch=forcedPortamentoAudioSmokeDetail::measurePitch(decoded,rate,-start+1.25);log+="rendered_re_midi="+juce::String(rePitch.midi,4)+" ga_midi="+juce::String(gaPitch.midi,4)+"\n";check("actual_native_target_pitch_retained_after_source_stretch",rePitch.voiced&&gaPitch.voiced&&std::abs(rePitch.midi-62)<1&&std::abs(gaPitch.midi-64)<1);
        }
        check("one_undo_restores_all_original_score_sources",project.undo()&&project.contentFingerprint()==before);pump();closeWarnings();
        // An imported note changed after picker start must never be overwritten.
        pianoRoll.setSelectedNoteIds({ids[0]});pianoRoll.applyNoteMenuChoice(ids[0],33,0,{});check("actual_menu33_uses_same_score_picker",wait([&]{return materialInputSession&&materialInputSession->ready&&picker();}));project.setNotesMidi({ids[0]},old[0].midiNote+1);const auto stale=project.contentFingerprint();choose(phrase,{"da"});closeWarnings();check("stale_score_picker_never_partially_replaces",wait([&]{return !materialInputSession;})&&project.contentFingerprint()==stale);project.undo();pump();closeWarnings();
        // Incremental one-note choices must join by the same shared source proof.
        for(int i=0;i<3;++i){pianoRoll.setSelectedNoteIds({ids[std::size_t(i)]});pianoRoll.applyNoteMenuChoice(ids[std::size_t(i)],33,0,{});check("incremental_menu33_source_choice",choose(i<2?phrase:gaFile,{i==0?"da":i==1?"re":"ga"})&&wait([&]{return !materialInputSession;})&&wait([&]{return pendingNativeAnalyses==0;},180000));}
        const auto incremental=project.snapshot().tracks[0].clips[0];check("incremental_same_WAV_shared_clock_no_fake_unused_phone",incremental.notes[0].clipPartId==incremental.notes[1].clipPartId&&incremental.notes[0].clipPartId!=incremental.notes[2].clipPartId&&std::none_of(incremental.notes.begin(),incremental.notes.end(),[](const auto& note){return note.materialRegionId=="mo";}));
        bool incrementalPitch=true;for(int i=0;i<3;++i){const auto& note=incremental.notes[std::size_t(i)];incrementalPitch&=note.sourcePitchMeasured&&note.sourceMidiCenter>=0&&std::any_of(note.contour.begin(),note.contour.end(),[](const auto& point){return point.voiced;});}
        check("incremental_rebinding_reuses_full_source_facts_not_unmeasured_contours",incrementalPitch);
        check("UST_missing_P_O_remain_unknown",file!=ust||(!incremental.notes[1].utauPreutteranceOverrideEnabled&&!incremental.notes[1].utauOverlapOverrideEnabled));check("USTX_missing_P_O_velocity_not_fabricated",file!=ustx||(!incremental.notes[0].utauPreutteranceOverrideEnabled&&!incremental.notes[0].utauOverlapOverrideEnabled&&incremental.notes[0].utauConsonantVelocity==inheritedUtauConsonantVelocity));
    }
    check("registered_PCM_and_OTTO_never_modified",juce::SHA256(phrase).toHexString()==phraseHash&&juce::SHA256(gaFile).toHexString()==gaHash&&juce::SHA256(manifest).toHexString()==ottoHash);return finish();
#endif
}
}
