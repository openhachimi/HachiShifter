#pragma once
#include "../MainComponent.h"
#include "../NativeOverlapConnection.h"
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
// Include once in MainComponent.cpp. Exercises production button and menu
// consumers against the live Main-owned model and the real PianoRoll view.
bool MainComponent::diagnosticNativeForceConnectionUi(const juce::File& output)
{
    const auto root=output.getChildFile("main-native-force-ui-"+juce::Uuid().toString());
    if(root.createDirectory().failed())return false;
    stopTimer();audio.stop();bool ok=true;juce::String log,error;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log+=name+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto finish=[&]{const bool saved=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"main_native_force_ui_fixture="<<root.getFullPathName()<<std::endl;return ok&&saved;};
#if !JUCE_WINDOWS
    check("actual_Windows_message_pump",false);return finish();
#else
    const auto pump=[&]
    {
        MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i)
        {if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}
        project.dispatchPendingMessages();pianoRoll.refreshFromModel();return true;
    };
    const auto wait=[&](const std::function<bool()>& ready)
    {const auto until=juce::Time::getMillisecondCounterHiRes()+3000;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<until){if(!pump())return false;juce::Thread::sleep(2);}pump();return ready();};
    const auto eq=[](double a,double b){return std::abs(a-b)<1.e-7;};
    juce::File sources[2]={root.getChildFile("a.wav"),root.getChildFile("b.wav")};
    for(int j=0;j<2;++j)
    {
        juce::AudioBuffer<float> pcm(1,67200);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,.05f*std::sin(juce::MathConstants<double>::twoPi*(j?391.995435981749:261.625565300599)*i/48000.0));
        auto stream=sources[j].createOutputStream();juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,24,{},0):nullptr);
        check("physical_source_fixture_"+juce::String(j),writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));
    }
    const auto hashA=juce::SHA256(sources[0]).toHexString(),hashB=juce::SHA256(sources[1]).toHexString();
    ProjectData seed;TrackData track;track.id="force-ui-native";track.trackType=TrackType::hachi;track.compose=true;
    track.nativeNsfAudio=true;track.pitchAlgorithm=PitchAlgorithm::world;track.normalizeVolume=false;track.allowNativeAudioOverlap=true;
    track.renderOrder=RenderOrder::processThenSplice;track.mergeContiguousSource=false;
    for(int i=0;i<2;++i)
    {
        ClipData clip;clip.id="clip-"+juce::String(i);clip.sourceFile=sources[i];clip.startSeconds=i*.75;clip.durationSeconds=clip.sourceDurationSeconds=1;
        clip.sourceOffsetSeconds=.1;clip.sourceTimeMap={{0,0},{1,1}};clip.fadeInSeconds=i?.13:.17;clip.fadeOutSeconds=i?.11:.07;
        NoteData note;note.id=i?"B":"A";note.label=i?"b":"a";note.durationSeconds=1;note.midiNote=note.sourceMidiCenter=i?67.f:60.f;
        note.sourcePitchMeasured=true;note.contour={{0,0,0,true},{1,0,0,true}};
        note.pitchControlPoints={{0,note.midiNote,PitchCurveShape::linear},{1,note.midiNote,PitchCurveShape::linear}};
        note.materialAudioId=i?"audio-b":"audio-a";note.materialRegionId=i?"row-b":"row-a";
        note.materialSourceStartSeconds=note.nativeSourceStartSeconds=.1;note.materialSourceEndSeconds=note.nativeSourceEndSeconds=1.1;
        note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.1;note.gain=.83f;
        note.amplitudeEnvelope={{0,-2},{.4,-1},{1,-3}};
        clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};
    const auto select=[&]
    {
        selectedTrackId=track.id;pianoRoll.setSourceEditMode(false);pianoRoll.setFocusedTrack(track.id);pianoRoll.setFocusedClip({});
        pianoRoll.refreshFromModel();pianoRoll.setSelectedNoteIds({"A","B"});refreshSpliceButton();
    };
    setSize(1320,850);resized();project.resetDocument(seed);project.dispatchPendingMessages();select();
    pianoRoll.setRowHeight(24);pianoRoll.setPixelsPerSecond(360);pianoRoll.setShowWaveforms(false);pianoRoll.setShowNativeRenderedWaveforms(false);
    pianoRoll.setShowNoteLabels(true);pianoRoll.setShowPhonemeInfo(false);pianoRoll.setReadsVoicebankInBackground(false);
    const auto before=project.contentFingerprint();const auto revision=project.revisionNumber();
    const auto seamIs=[&](double time){pianoRoll.refreshFromModel();const auto seams=pianoRoll.nativeForcedSeams();return seams.size()==1&&eq(seams[0].second,time);};
    const auto activeBridge=[&](const ProjectData& data,double from,double to)
    {if(data.tracks.empty())return false;const auto curves=resolvedConnectionPitchCurves(data.tracks[0],data.nativeConnections);return curves.size()==1&&curves[0].active
        &&eq(curves[0].fromSeconds,from)&&eq(curves[0].toSeconds,to)&&eq(curves[0].points.front().targetMidi,60)&&eq(curves[0].points.back().targetMidi,67);};
    const auto capture=[&](const juce::String& name,bool checkOpenGap)
    {
        pianoRoll.refreshFromModel();const int cropY=std::max(0,int(pianoRoll.diagnosticYForMidi(67))-34);
        pianoViewport.setViewPosition(0,cropY);juce::Image image(juce::Image::ARGB,980,260,true,juce::SoftwareImageType{});
        {juce::Graphics graphics(image);graphics.setOrigin(0,-cropY);pianoRoll.paintEntireComponent(graphics,true);}
        auto out=root.getChildFile(name+".png").createOutputStream();check(name+"_software_PNG",out&&juce::PNGImageFormat().writeImageToStream(image,*out));
        // Software paint must contain the actual rows, not a black GPU surface.
        const auto first=image.getPixelAt(0,0);int varying=0;for(int y=0;y<image.getHeight();y+=4)for(int x=0;x<image.getWidth();x+=4)varying+=image.getPixelAt(x,y)!=first;
        check(name+"_nonblank_real_component",varying>100);
        if(checkOpenGap)
        {
            const auto seams=pianoRoll.nativeForcedSeams();if(seams.size()!=1){check(name+"_open_partial_cutline",false);return;}
            const int x=juce::roundToInt(pianoRoll.diagnosticEdgeX(seams[0].second));
            const double top=pianoRoll.diagnosticYForMidi(67)-12+2-cropY,bottom=pianoRoll.diagnosticYForMidi(60)+12-2-cropY;
            const auto middle=(top+bottom)*.5;const auto accent=Palette::accentLight;
            const auto accented=[&](int y)
            {
                for(int dx=-1;dx<=1;++dx)if(x+dx>=0&&x+dx<image.getWidth()&&y>=0&&y<image.getHeight())
                {const auto c=image.getPixelAt(x+dx,y);const int distance=std::abs(int(c.getRed())-int(accent.getRed()))+std::abs(int(c.getGreen())-int(accent.getGreen()))+std::abs(int(c.getBlue())-int(accent.getBlue()));if(distance<45)return true;}
                return false;
            };
            int upper=0,lower=0,gap=0;for(int y=int(top)+4;y<int(middle)-5;++y)upper+=accented(y);
            for(int y=int(middle)+5;y<int(bottom)-4;++y)lower+=accented(y);
            for(int y=int(middle)-1;y<=int(middle)+1;++y)gap+=accented(y);
            check(name+"_open_partial_cutline",upper>20&&lower>20&&gap==0);
        }
    };
    check("actual_toolbar_control_ID_enabled",spliceButton.getComponentID()=="native.forceConnection"&&spliceButton.isEnabled());
    spliceButton.triggerClick();check("toolbar_actual_async_user_click_dispatched",wait([&]{return project.revisionNumber()!=revision;}));
    const auto joined=project.snapshot();
    const bool compound=joined.tracks.size()==1&&joined.tracks[0].clips.size()==1&&joined.tracks[0].clips[0].nativeAudioLinked
        &&joined.tracks[0].clips[0].parts.size()==2&&joined.nativeConnections.size()==1&&joined.nativeConnections[0].type=="native-overlap";
    check("toolbar_routes_native_force_not_old_pitch_only_overlap_rejection",compound&&project.revisionNumber()==revision+1);
    check("toolbar_computes_live_60_to_67_bridge",activeBridge(joined,.825,.925));
    check("PianoRoll_real_expanded_view_retains_unexpanded_proof_seam",seamIs(.875));
    auto chosen=pianoRoll.selectedNoteIds();std::sort(chosen.begin(),chosen.end());check("toolbar_retains_actual_selection",chosen==std::vector<juce::String>{"A","B"});
    bool preserved=compound;
    if(compound)
    {
        const auto expanded=expandedClipParts(joined.tracks[0].clips[0]);preserved=expanded.size()==2;
        for(std::size_t i=0;preserved&&i<expanded.size();++i)
        {
            const auto& a=seed.tracks[0].clips[i];const auto& b=expanded[i];
            preserved&=a.sourceFile==b.sourceFile&&eq(a.startSeconds,b.startSeconds)&&eq(a.durationSeconds,b.durationSeconds)
                &&eq(a.sourceOffsetSeconds,b.sourceOffsetSeconds)&&eq(a.sourceDurationSeconds,b.sourceDurationSeconds)&&eq(a.fadeInSeconds,b.fadeInSeconds)&&eq(a.fadeOutSeconds,b.fadeOutSeconds)
                &&b.notes.size()==1&&a.notes[0].id==b.notes[0].id&&eq(a.notes[0].startSeconds,b.notes[0].startSeconds)&&eq(a.notes[0].durationSeconds,b.notes[0].durationSeconds)
                &&a.notes[0].midiNote==b.notes[0].midiNote&&eq(a.notes[0].gain,b.notes[0].gain)&&b.notes[0].pitchControlPoints.size()==2&&b.notes[0].amplitudeEnvelope.size()==3
                &&eq(b.notes[0].materialSourceStartSeconds,.1)&&eq(b.notes[0].materialSourceEndSeconds,1.1);
            for(std::size_t k=0;preserved&&k<a.notes[0].pitchControlPoints.size();++k)
                preserved&=eq(a.notes[0].pitchControlPoints[k].timeSeconds,b.notes[0].pitchControlPoints[k].timeSeconds)
                    &&a.notes[0].pitchControlPoints[k].targetMidi==b.notes[0].pitchControlPoints[k].targetMidi;
            for(std::size_t k=0;preserved&&k<a.notes[0].amplitudeEnvelope.size();++k)
                preserved&=eq(a.notes[0].amplitudeEnvelope[k].timeSeconds,b.notes[0].amplitudeEnvelope[k].timeSeconds)
                    &&a.notes[0].amplitudeEnvelope[k].gainDb==b.notes[0].amplitudeEnvelope[k].gainDb;
            preserved&=a.sourceTimeMap.size()==b.sourceTimeMap.size();
            for(std::size_t k=0;preserved&&k<a.sourceTimeMap.size();++k)
                preserved&=eq(a.sourceTimeMap[k].targetSeconds,b.sourceTimeMap[k].targetSeconds)&&eq(a.sourceTimeMap[k].sourceSeconds,b.sourceTimeMap[k].sourceSeconds);
        }
    }
    check("actual_UI_preserves_full_score_body_sources_controls_author_fades",preserved);capture("toolbar-native-overlap-joined",false);
    check("toolbar_force_is_single_undo",project.undo()&&!project.canUndo()&&project.contentFingerprint()==before);pump();select();
    const auto menuIds=pianoRoll.diagnosticEnabledNoteMenuIds("A");check("actual_note_menu_24_enabled",std::find(menuIds.begin(),menuIds.end(),24)!=menuIds.end());
    const auto menuRevision=project.revisionNumber();pianoRoll.applyNoteMenuChoice("A",24,0,{"A","B"});pump();
    const auto menuJoined=project.snapshot();check("actual_menu_24_uses_same_atomic_native_core",menuJoined.nativeConnections.size()==1&&menuJoined.nativeConnections[0].type=="native-overlap"
        &&menuJoined.tracks[0].clips.size()==1&&menuJoined.tracks[0].clips[0].nativeAudioLinked&&project.revisionNumber()==menuRevision+1&&activeBridge(menuJoined,.825,.925)&&seamIs(.875));
    check("menu_force_is_single_undo",project.undo()&&!project.canUndo()&&project.contentFingerprint()==before);pump();select();
    spliceButton.triggerClick();check("toolbar_reestablishes_after_menu_undo",wait([&]{return project.snapshot().nativeConnections.size()==1;}));
    check("actual_native_group_move",project.moveNativeNotes({"A","B"},.4,0,&error));pump();
    check("actual_GUI_cutline_and_graph_follow_live_group_move",seamIs(1.275)&&activeBridge(project.snapshot(),1.225,1.325));capture("toolbar-native-overlap-moved",false);
    const auto saved=root.getChildFile("actual-main-forced-moved.hjpx");ProjectModel cold;
    const bool coldReady=project.save(saved,error)&&cold.load(saved,error);check("actual_UI_HJPX_cold_roundtrip",coldReady);if(!coldReady)return finish();
    project.resetDocument(cold.snapshot());project.dispatchPendingMessages();select();check("cold_reopen_GUI_uses_live_proof_cutline",seamIs(1.275)&&activeBridge(project.snapshot(),1.225,1.325));
    const auto pitched=cold.snapshot();
    const auto disableRevision=project.revisionNumber();spliceButton.triggerClick();
    check("second_toolbar_user_click_disables_pitch",wait([&]{const auto d=project.snapshot();return d.nativeConnections.size()==1&&d.nativeConnections[0].pitchCurve.empty();}));
    const auto disabled=project.snapshot();check("pitch_disable_keeps_physical_compound_crossfade_and_live_GUI_line",disabled.tracks[0].clips.size()==1&&disabled.tracks[0].clips[0].nativeAudioLinked
        &&disabled.nativeConnections[0].type=="native-overlap"&&project.revisionNumber()==disableRevision+1
        &&eq(disabled.tracks[0].clips[0].parts[0].crossfadeOutSeconds,.25)&&eq(disabled.tracks[0].clips[0].parts[1].crossfadeInSeconds,.25)&&seamIs(1.275));
    capture("toolbar-native-overlap-pitch-disabled-open-cutline",true);
    // Use the same actual renderer/cache for on -> off -> on. The source
    // controls are known C4/G4 sines, not an ASR or human-voice oracle.
    AudioEngine rendered(false);rendered.prepareToPlay(257,48000);
    const auto render=[&](const ProjectData& data,const juce::String& name,juce::AudioBuffer<float>& pcm)
    {
        rendered.syncProject(data);
        if(!wait([&]{const auto p=rendered.renderProgress();return !p||*p>=.999;}))return false;
        log+=name+"_backend="+rendered.activeRenderBackends()+" warning="+rendered.activeRenderWarnings()+"\n";
        const auto file=root.getChildFile(name+".wav");juce::String issue;
        if(!rendered.exportWav(file,issue)||rendered.activeRenderWarnings().isNotEmpty())return false;
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if(!reader)return false;pcm.setSize(2,int(reader->lengthInSamples));return reader->read(&pcm,0,pcm.getNumSamples(),0,true,true);
    };
    juce::AudioBuffer<float> pitchOn,pitchOff,pitchOnAgain;
    const bool onReady=render(pitched,"actual-WORLD-overlap-pitch-on",pitchOn),offReady=render(disabled,"actual-WORLD-overlap-pitch-off",pitchOff),againReady=render(pitched,"actual-WORLD-overlap-pitch-on-hot-cache",pitchOnAgain);
    check("actual_native_overlap_WORLD_renders_on_off_on",onReady&&offReady&&againReady);
    float pitchDifference=0;bool cacheExact=onReady&&againReady&&pitchOn.getNumSamples()==pitchOnAgain.getNumSamples();
    if(onReady&&offReady&&pitchOn.getNumSamples()==pitchOff.getNumSamples())for(int i=int(1.225*48000);i<std::min(int(1.325*48000),pitchOn.getNumSamples());++i)
        pitchDifference=std::max(pitchDifference,std::abs(pitchOn.getSample(0,i)-pitchOff.getSample(0,i)));
    if(cacheExact)for(int channel=0;channel<2;++channel)cacheExact&=std::memcmp(pitchOn.getReadPointer(channel),pitchOnAgain.getReadPointer(channel),std::size_t(pitchOn.getNumSamples())*sizeof(float))==0;
    log+="actual_bridge_PCM_peak_difference="+juce::String(pitchDifference,9)+"\n";
    check("actual_overlap_bridge_reaches_native_audio_not_only_model",pitchDifference>1.e-5f);
    check("native_overlap_pitch_cache_distinguishes_on_off_and_restores_exact_on",cacheExact);
    // A score/protocol-only native track must NOT be mistaken for two physical
    // WAV owners by the toolbar branch; its existing pitch-only API stays intact.
    auto protocol=seed;for(std::size_t i=0;i<protocol.tracks[0].clips.size();++i)
    {auto& clip=protocol.tracks[0].clips[i];clip.sourceFile=juce::File{};clip.startSeconds=double(i);clip.notes[0].materialAudioId.clear();clip.notes[0].materialRegionId.clear();clip.notes[0].utauPreutteranceSeconds=0;}
    project.resetDocument(protocol);project.dispatchPendingMessages();select();const auto fallbackRevision=project.revisionNumber();spliceButton.triggerClick();
    check("protocol_toolbar_fallback_click_dispatched",wait([&]{return project.revisionNumber()!=fallbackRevision;}));
    const auto fallback=project.snapshot();check("scoreprotocol_retains_old_pitch_only_not_source_assembly",fallback.nativeConnections.size()==1&&fallback.nativeConnections[0].type=="pitch-portamento"
        &&fallback.tracks[0].clips.size()==2&&fallback.tracks[0].clips[0].parts.empty()&&fallback.tracks[0].clips[1].parts.empty()&&pianoRoll.nativeForcedSeams().empty());
    check("no_source_analysis_requested_by_force_UI",pendingNativeAnalyses==0);
    check("source_PCM_unchanged_by_real_GUI_actions",juce::SHA256(sources[0]).toHexString()==hashA&&juce::SHA256(sources[1]).toHexString()==hashB);
    return finish();
#endif
}
}
