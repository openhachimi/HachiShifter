#pragma once
#include "../NativeSourceTimeMap.h"
#include "../ClipParts.h"
#include "../NativePitchIdentity.h"
namespace hachi
{
inline bool MainComponent::diagnosticNativeSourcePitch(const juce::File& folder,
                                                      const juce::File& models)
{
    folder.createDirectory(); stopTimer(); bool ok = true; int checks = 0;
    const auto check = [&](const char* name, bool pass)
    { ++checks; ok &= pass; std::cout << name << '=' << pass << std::endl; };
    const auto near = [](double a, double b) { return std::abs(a-b) < 1.0e-4; };
    const auto source = folder.getChildFile("vibrato.wav");
    SampleSettings::sidecarFor(source).deleteFile();
    juce::File(source.getFullPathName()+".hachi.csv").deleteFile();
    juce::AudioBuffer<float> samples(1, 96000); double phase = 0;
    for (int i = 0; i < samples.getNumSamples(); ++i)
    {
        const auto t = i/48000.0;
        const auto hz = 200.0 * std::exp2(.35 * std::sin(t * juce::MathConstants<double>::twoPi * 6) / 12);
        phase += juce::MathConstants<double>::twoPi * hz / 48000;
        samples.setSample(0, i, static_cast<float>(.3 * (std::sin(phase)
            + .3*std::sin(phase*2) + .15*std::sin(phase*3))));
    }
    auto out = source.createOutputStream(); if (out) { out->setPosition(0); out->truncate(); }
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(out.get(),48000,1,16,{},0));
    if (!writer) { check("fixture_written",false); return false; }
    out.release(); check("fixture_written",writer->writeFromAudioSampleBuffer(samples,0,samples.getNumSamples())); writer.reset();
    backend::AnalysisConfig config; config.gameModelDirectory=models.getChildFile("game/medium");
    config.fcpeModelPath=models.getChildFile("fcpe/fcpe.onnx"); config.inference=backend::InferenceBackend::cpu;
    juce::String error; auto analysis=backend::AnalysisService::analyse(source,config,error);
    check("real_analysis_models_available",!analysis.notes.empty()&&analysis.status.gameModelReady
        &&analysis.status.fcpeModelReady&&analysis.status.onnxRuntimeReady);
    std::cout<<"tone_analysis_backend="<<analysis.status.activeBackend
             <<"; warning="<<analysis.warning<<std::endl;
    if (analysis.notes.empty()) return false;
    float low=1e9f,high=-1e9f; int frames=0;
    for (const auto& n:analysis.notes) for (const auto& p:n.contour)
        if(p.voiced&&n.startSeconds+p.timeSeconds>.15&&n.startSeconds+p.timeSeconds<1.85)
        {auto cents=n.sourceMidiCenter*100+p.relativeCents;low=std::min(low,cents);high=std::max(high,cents);++frames;}
    check("measured_pitch_contains_real_vibrato",frames>100&&high-low>30);
    auto fcpe=backend::FcpeAnalyzer::analyse(source,config.fcpeModelPath,
        {backend::InferenceBackend::cpu,-1,2},error);
    float fcpeLow=1e9f,fcpeHigh=-1e9f;int fcpeFrames=0;
    for(const auto& p:fcpe)if(p.voiced&&p.timeSeconds>.15&&p.timeSeconds<1.85)
    {fcpeLow=std::min(fcpeLow,p.midi);fcpeHigh=std::max(fcpeHigh,p.midi);++fcpeFrames;}
    check("fcpe_measures_vibrato_on_frame_grid",fcpeFrames>100&&fcpeHigh-fcpeLow>.3f);
    // No annotation: GAME supplies regions, FCPE supplies each measured frame.
    ProjectModel fresh; const auto freshId=fresh.addAudioFile(source,2);
    const auto initial=fresh.snapshot().tracks[0].clips[0];
    fresh.resizeClip(freshId,3,4);
    check("analysis_survives_stretch_while_pending",fresh.setClipAudioAnalysis(freshId,analysis.notes,initial));
    const auto detected=fresh.snapshot().tracks[0].clips[0];
    check("new_audio_starts_at_measured_pitch",!detected.notes.empty()&&std::all_of(detected.notes.begin(),detected.notes.end(),
        [&](const auto& n){return n.nativeUnpitched || (near(n.midiNote,n.sourceMidiCenter)&&n.contour.size()>10);}));
    check("new_audio_keeps_stretched_duration",near(detected.startSeconds,3)&&near(detected.durationSeconds,4));
    // Use exact source F0 to verify mapping independently of model accuracy.
    NoteData raw;raw.id="raw";raw.durationSeconds=1;raw.sourceMidiCenter=61.3f;raw.midiNote=61.3f;
    raw.sourcePitchMeasured=true;
    for(int i=0;i<=200;++i){double t=i*.005;float cents=100*std::sin(static_cast<float>(t*18*juce::MathConstants<double>::twoPi));
        raw.contour.push_back({t,cents,cents,!(t>.45&&t<.55)});}
    ClipData clip;clip.id="clip";clip.sourceFile=source;clip.durationSeconds=2;clip.sourceDurationSeconds=1;
    clip.sourceTimeMap={{0,0},{1,.25},{2,1}};
    NoteData note;note.id="note";note.label="authored";note.durationSeconds=2;note.sourceMidiCenter=-1;
    note.consonantSeconds=0;note.gain=.7f;note.formantSemitones=2;
    note.nativeSegments={{"a","onset",NativeSegmentRole::consonant,0,.2}};
    note.amplitudeEnvelope={{0,-3},{1,0}};note.contour={{0,0,0,true},{2,0,0,true}};clip.notes={note};
    TrackData track;track.id="track";track.compose=true;track.pitchAlgorithm=PitchAlgorithm::world;track.clips={clip};
    ProjectData data;data.tracks={track};ProjectModel model;model.replace(data);
    check("authored_regions_receive_dense_f0",model.setClipAudioAnalysis(clip.id,{raw},clip));
    auto measured=model.snapshot().tracks[0].clips[0];const auto& n=measured.notes[0];
    check("regions_labels_segments_controls_preserved",n.id==note.id&&n.label==note.label&&near(n.durationSeconds,2)
        &&n.nativeSegments.size()==1&&near(n.gain,.7)&&near(n.formantSemitones,2)&&n.amplitudeEnvelope.size()==2);
    check("unpitched_oto_row_adopts_measured_center",near(n.midiNote,n.sourceMidiCenter)&&std::abs(n.midiNote-60)>.5);
    const auto pitchAt=[&](const NoteData& value,double t){const auto it=std::min_element(value.contour.begin(),value.contour.end(),
        [&](const auto& a,const auto& b){return std::abs(a.timeSeconds-t)<std::abs(b.timeSeconds-t);});
        return value.sourceMidiCenter+it->relativeCents/100;};
    check("manual_warp_f0_matches_source_clock",near(pitchAt(n,.6),raw.sourceMidiCenter+raw.contour[30].relativeCents/100)
        &&near(nativeTargetTimeAt(nativeSourceTimeMap(clip),.15),.6));
    check("unvoiced_span_does_not_become_flat_pitch",!n.contour[static_cast<std::size_t>(std::lround((1+(.5-.25)/.75)/.005))].voiced);
    check("dense_frame_resolution",n.contour.size()==401&&near(n.contour.back().timeSeconds,2));
    auto noise=clip;noise.notes[0].midiNote=62;auto unvoiced=raw;
    for(auto& p:unvoiced.contour)p.voiced=false;
    backend::AnalysisService::applySourcePitch(noise,{unvoiced});
    check("unvoiced_region_keeps_unknown_center_and_authored_offset",near(noise.notes[0].midiNote,62)
        &&noise.notes[0].sourceMidiCenter<0&&std::none_of(noise.notes[0].contour.begin(),noise.notes[0].contour.end(),
            [](const auto& p){return p.voiced;}));
    model.resizeClip(clip.id,4,4);auto stretched=model.snapshot().tracks[0].clips[0];
    check("clip_stretch_preserves_measured_frames",stretched.notes[0].contour.size()==401
        &&near(pitchAt(stretched.notes[0],1.2),pitchAt(n,.6))&&near(stretched.notes[0].contour.back().timeSeconds,4));
    model.undo();check("stretch_undo_keeps_real_f0",near(model.snapshot().tracks[0].clips[0].notes[0].contour.back().timeSeconds,2));
    const auto saved=folder.getChildFile("real-pitch.hachi");check("project_saved",model.save(saved,error));
    ProjectModel reopened;check("project_reopened",reopened.load(saved,error));
    check("saved_project_keeps_dense_f0",reopened.snapshot().tracks[0].clips[0].notes[0].contour.size()==401);
    check("saved_project_keeps_measured_source_provenance",reopened.snapshot().tracks[0].clips[0].notes[0].sourcePitchMeasured);
    // F0 refresh retains manual targets and authored target pitch.
    auto manual=clip;manual.notes[0].sourceMidiCenter=61;manual.notes[0].midiNote=65;
    for(auto& p:manual.notes[0].contour){p.hasManualTarget=true;p.manualTargetCents=25;}
    check("source_refresh_keeps_manual_edits",backend::AnalysisService::applySourcePitch(manual,{raw})==1
        &&near(manual.notes[0].midiNote,65)&&manual.notes[0].contour[50].hasManualTarget
        &&near(renderedPitchCents(manual.notes[0],manual.notes[0].contour[50]),25));
    auto pending=clip;pending.notes.clear();model.replace(data);
    check("pending_analysis_does_not_replace_user_notes",!model.setClipAudioAnalysis(clip.id,{raw},pending));
    auto backing=data;backing.tracks[0].accompaniment=true;model.replace(backing);
    check("accompaniment_untouched",!model.setClipAudioAnalysis(clip.id,{raw},clip));
    auto utau=data;utau.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;model.replace(utau);
    check("utau_authored_pitch_untouched",!model.setClipAudioAnalysis(clip.id,{raw},clip)
        &&model.snapshot().tracks[0].clips[0].notes[0].contour.size()==2);
    // Analysis-only boundaries must retain the recording's own transition,
    // rather than replacing it with the automatic transition for authored notes.
    auto natural=clip;natural.durationSeconds=1;natural.sourceDurationSeconds=1;
    natural.sourceTimeMap={{0,0},{1,1}};natural.notes.clear();
    for(int i=0;i<2;++i){auto part=raw;part.id=juce::String(i);part.startSeconds=i*.5;
        part.durationSeconds=.5;part.midiNote=part.sourceMidiCenter=60+i*12;
        part.contour={{0,20,20,true},{.5,-20,-20,true}};natural.notes.push_back(part);}
    auto naturalData=data;naturalData.tracks[0].clips={natural};
    const auto naturalTarget=AudioEngine::diagnosticNativeTargetMidi(naturalData,0,0);
    check("analysis_seam_keeps_source_pitch",near(naturalTarget[99],59.804)&&near(naturalTarget[101],72.196));
    auto unknown=natural.notes[0];unknown.sourcePitchMeasured=false;
    check("hand_drawn_absolute_target_is_not_measured_source",!nativeSourcePitchIsKnown(unknown));
    // Compare actual decoded samples, including source crop and both channels.
    const auto stereoFile=folder.getChildFile("identity-stereo.wav");
    juce::AudioBuffer<float> stereo(2,48000);
    for(int i=0;i<48000;++i){stereo.setSample(0,i,samples.getSample(0,i));
        stereo.setSample(1,i,static_cast<float>(.13*std::sin(i*.077)+.02*std::cos(i*.153)));}
    auto stereoStream=stereoFile.createOutputStream();
    std::unique_ptr<juce::AudioFormatWriter> stereoWriter(format.createWriterFor(stereoStream.get(),48000,2,16,{},0));
    if(!stereoWriter){check("identity_stereo_fixture_written",false);return false;}
    stereoStream.release();stereoWriter->writeFromAudioSampleBuffer(stereo,0,48000);stereoWriter.reset();
    juce::AudioFormatManager readers;readers.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(readers.createReaderFor(stereoFile));
    juce::AudioBuffer<float> expected(2,24000);reader->read(&expected,0,24000,6000,true,true);
    backend::Mld5FileRenderRequest identity;identity.sourceFile=stereoFile;
    identity.sourceOffsetSeconds=.125;identity.sourceDurationSeconds=.5;identity.targetDurationSeconds=.5;
    identity.preserveUneditedSource=true;identity.timeMap={{0,0},{.5,.5}};
    identity.sourceMidi.assign(101,57);identity.targetMidi=identity.sourceMidi;
    identity.noteGain.assign(101,1);identity.breath.assign(101,0);
    identity.hifiganModelDirectory=models.getChildFile("nsf_hifigan");identity.inference.requested=backend::InferenceBackend::cpu;
    const auto runFile=[&](backend::Mld5FileRenderRequest request){
        backend::RenderService service;backend::RenderedAudio result;std::atomic<bool> done{false};
        service.renderMld5File(std::move(request),[&](auto audio){result=std::move(audio);done.store(true,std::memory_order_release);});
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000;
        while(!done.load(std::memory_order_acquire)&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(10);
        service.cancelAll();return result;};
    const auto exact=[&](const auto& result,float gain){
        if(result.buffer.getNumSamples()!=24000||result.buffer.getNumChannels()!=2||result.sampleRate!=48000)return false;
        for(int ch=0;ch<2;++ch)for(int i=0;i<24000;++i)
            if(std::abs(result.buffer.getSample(ch,i)-expected.getSample(ch,i)*gain)>1e-7f)return false;
        return true;};
    for(const auto backend:{backend::PitchRenderBackend::nsfHifigan,backend::PitchRenderBackend::world,
                          backend::PitchRenderBackend::mld5,backend::PitchRenderBackend::llsm2}){
        identity.pitchBackend=backend;const auto result=runFile(identity);
        check(("unedited_stereo_pcm_backend_"+juce::String(static_cast<int>(backend))).toRawUTF8(),
              result.backend=="native-source-preserved"&&exact(result,1));}
    identity.pitchBackend=backend::PitchRenderBackend::world;
    auto gainOnly=identity;gainOnly.noteGain.assign(101,.7f);
    check("gain_edit_uses_original_pcm",exact(runFile(gainOnly),.7f));
    for(int edit=0;edit<7;++edit){auto changed=identity;
        if(edit==0)changed.targetMidi.assign(101,60);
        if(edit==1)changed.timeMap={{0,0},{.25,.35},{.5,.5}};
        if(edit==2)changed.formantSemitones.assign(101,2);
        if(edit==3)changed.tension.assign(101,.4f);
        if(edit==4)changed.normalizeVolume=true;
        if(edit==5)changed.robustPitchCurve.assign(101,1);
        if(edit==6){changed.preserveUneditedSource=false;changed.pitchBackend=backend::PitchRenderBackend::nsfHifigan;}
        const auto result=runFile(changed);
        check(("explicit_edit_keeps_renderer_"+juce::String(edit)).toRawUTF8(),
              result.buffer.getNumSamples()==24000&&result.backend!="native-source-preserved");}
    // Actual native piano roll must draw all F0 frames, even in the point tool.
    data.tracks[0].clips={measured};model.replace(data);
    PianoRollComponent roll(model,strings);roll.setFocusedTrack(track.id);roll.setFocusedClip(clip.id);
    roll.setPixelsPerSecond(300);roll.setRowHeight(28);roll.setSize(850,roll.getHeight());
    roll.setShowNativeWaveforms(false);roll.setShowPitchLine(true);
    const auto capture=[&]{return roll.createComponentSnapshot({0,0,850,roll.getHeight()});};
    const auto difference=[](const juce::Image& a,const juce::Image& b,juce::Rectangle<int> area)
    {int count=0;area=area.getIntersection(a.getBounds());for(int y=area.getY();y<area.getBottom();++y)
        for(int x=area.getX();x<area.getRight();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))++count;return count;};
    const auto saveImage=[&](const char* name,const juce::Image& image){auto stream=folder.getChildFile(juce::String(name)+".png").createOutputStream();
        if(!stream)return false;stream->setPosition(0);stream->truncate();return juce::PNGImageFormat().writeImageToStream(image,*stream);};
    const auto on=capture();roll.setShowPitchLine(false);const auto off=capture();
    check("pitch_display_toggle_draws_native_f0",difference(on,off,on.getBounds())>500);
    check("native_f0_has_vertical_detail",difference(on,off,{220,static_cast<int>(roll.diagnosticNoteY(0))-20,180,15})>20);
    check("native_pitch_image_written",saveImage("native-game-pitch",on));
    roll.setShowPitchLine(true);roll.setTool(PianoRollComponent::Tool::points);const auto points=capture();
    roll.setShowPitchLine(false);check("point_tool_retains_measured_f0",difference(points,capture(),points.getBounds())>500);
    roll.setTool(PianoRollComponent::Tool::note);roll.setShowPitchLine(true);roll.setPixelsPerSecond(600);
    const auto zoom=capture();roll.setShowPitchLine(false);
    check("zoom_keeps_dense_f0_visible",difference(zoom,capture(),zoom.getBounds())>500);
    check("zoom_pitch_image_written",saveImage("native-game-pitch-zoom",zoom));
    // Independently compare both traces after a real flatten edit.  The original
    // is deliberately curved, so the dashed source cannot pass as the flat target.
    roll.setPixelsPerSecond(300);roll.setTool(PianoRollComponent::Tool::note);
    roll.flattenPitchLine(n.id);model.dispatchPendingMessages();roll.diagnosticRefresh();
    const auto flattenedContour=model.snapshot().tracks[0].clips[0].notes[0].contour;
    const auto revisionBeforeView=model.revisionNumber();
    roll.setShowPitchLine(true);roll.setShowOriginalPitchLine(true);const auto both=capture();
    roll.setShowOriginalPitchLine(false);const auto targetOnly=capture();
    check("original_switch_hides_curved_source_keeps_flat_target",difference(both,targetOnly,both.getBounds())>300);
    roll.setShowPitchLine(false);const auto neither=capture();
    check("target_remains_independent_of_original_switch",difference(targetOnly,neither,neither.getBounds())>300);
    roll.setShowOriginalPitchLine(true);const auto originalOnly=capture();
    check("original_can_show_without_target",difference(originalOnly,neither,neither.getBounds())>300);
    roll.setShowPitchLine(true);check("both_traces_restore_identically",difference(both,capture(),both.getBounds())==0);
    roll.setTool(PianoRollComponent::Tool::points);const auto pointBoth=capture();
    roll.setShowOriginalPitchLine(false);const auto pointTarget=capture();
    check("point_tool_can_toggle_original_reference",difference(pointBoth,pointTarget,pointBoth.getBounds())>300);
    roll.setShowOriginalPitchLine(true);roll.setTool(PianoRollComponent::Tool::note);
    const auto afterView=model.snapshot().tracks[0].clips[0].notes[0].contour;
    bool unchanged=flattenedContour.size()==afterView.size();
    for(std::size_t i=0;unchanged&&i<afterView.size();++i)
        unchanged=near(afterView[i].relativeCents,flattenedContour[i].relativeCents)
            &&afterView[i].hasManualTarget==flattenedContour[i].hasManualTarget
            &&near(afterView[i].manualTargetCents,flattenedContour[i].manualTargetCents);
    check("display_switches_leave_source_and_playback_targets_unchanged",unchanged&&model.revisionNumber()==revisionBeforeView);
    check("original_and_flat_target_image_written",saveImage("original-and-flat-target",both));
    model.undo();model.dispatchPendingMessages();roll.diagnosticRefresh();
    // Default native envelope is a view of unity, not a synthesized UTAU fade.
    auto plain=data;plain.tracks[0].clips[0].notes[0].amplitudeEnvelope.clear();
    plain.tracks[0].clips[0].notes[0].amplitudeEnvelopeBasePercent=100;
    model.replace(plain);model.dispatchPendingMessages();roll.diagnosticRefresh();roll.setShowEnvelope(true);roll.setShowPitchLine(true);
    const auto flat=roll.tailFadeBaseEnvelope(n.id);
    check("native_default_envelope_is_unity_at_both_edges",flat.size()==2
        &&near(flat.front().timeSeconds,0)&&near(flat.back().timeSeconds,n.durationSeconds)
        &&near(flat.front().gainDb,0)&&near(flat.back().gainDb,0));
    check("displaying_default_does_not_write_audio_edit",model.snapshot().tracks[0].clips[0].notes[0].amplitudeEnvelope.empty());
    check("native_unity_envelope_image_written",saveImage("native-default-unity",capture()));
    plain.tracks[0].clips[0].notes[0].amplitudeEnvelopeBasePercent=50;model.replace(plain);model.dispatchPendingMessages();roll.diagnosticRefresh();
    const auto scaled=roll.tailFadeBaseEnvelope(n.id);
    check("native_explicit_base_stays_flat",scaled.size()==2&&near(scaled.front().gainDb,-6.0206)
        &&near(scaled.back().gainDb,-6.0206));
    plain.tracks[0].clips[0].notes[0].amplitudeEnvelopeBasePercent=100;
    plain.tracks[0].clips[0].notes[0].amplitudeEnvelope={{0,-12},{.3,0},{n.durationSeconds,-9}};
    model.replace(plain);model.dispatchPendingMessages();roll.diagnosticRefresh();const auto authored=roll.tailFadeBaseEnvelope(n.id);
    check("native_authored_envelope_is_preserved",authored.size()==3&&near(authored[0].gainDb,-12)
        &&near(authored[1].timeSeconds,.3)&&near(authored[2].gainDb,-9));
    plain.tracks[0].pitchAlgorithm=PitchAlgorithm::utau;
    plain.tracks[0].clips[0].notes[0].amplitudeEnvelope.clear();model.replace(plain);model.dispatchPendingMessages();roll.diagnosticRefresh();
    const auto utauDefault=roll.tailFadeBaseEnvelope(n.id);
    check("utau_default_keeps_attack_and_release",utauDefault.size()==4
        &&near(utauDefault.front().gainDb,-60)&&near(utauDefault.back().gainDb,-60));
    // Reproduce the screenshot through a real same-name OTO/HJM import.
    // Its consonant/vowel regions are metadata within one note, not GAME notes.
    SampleRegionSetting syllable;syllable.name="baip";syllable.regionEndSeconds=2;
    syllable.alignmentSeconds=.55;syllable.fixedDurationSeconds=.8;syllable.provenance="utau";
    check("same_syllable_sidecar_saved",SampleSettings::save(source,{syllable},error));
    ProjectModel syllableModel;const auto syllableClipId=syllableModel.addAudioFile(source,2);
    auto syllableData=syllableModel.snapshot();auto imported=syllableData.tracks[0].clips[0];
    check("same_alias_cv_is_one_note",imported.notes.size()==1&&imported.notes[0].nativeSegments.size()==2
        &&imported.notes[0].nativeSegments[0].alias=="baip"&&imported.notes[0].nativeSegments[1].alias=="baip");
    check("game_refresh_accepts_authored_syllable",syllableModel.setClipAudioAnalysis(syllableClipId,analysis.notes,imported));
    syllableData=syllableModel.snapshot();
    check("game_refresh_does_not_split_authored_syllable",syllableData.tracks[0].clips[0].notes.size()==1
        &&syllableData.tracks[0].clips[0].notes[0].nativeSegments.size()==2
        &&syllableData.tracks[0].clips[0].notes[0].contour.size()>100);
    PianoRollComponent syllableRoll(syllableModel,strings);
    syllableRoll.setFocusedTrack(syllableData.tracks[0].id);syllableRoll.setFocusedClip(syllableClipId);
    syllableRoll.setPixelsPerSecond(320);syllableRoll.setRowHeight(26);syllableRoll.setSize(850,syllableRoll.getHeight());
    syllableRoll.setPlayheadSeconds(2.4); // keep the red cursor off the edge under test
    syllableRoll.setShowNoteRange(false);syllableRoll.setShowEnvelope(true);
    syllableRoll.setShowNativeWaveforms(false);syllableRoll.setShowWaveforms(false);syllableRoll.setShowPitchLine(true);
    const auto syllableCapture=[&]{return syllableRoll.createComponentSnapshot({0,0,850,syllableRoll.getHeight()});};
    const auto imageArea=juce::Rectangle<int>(0,static_cast<int>(syllableRoll.diagnosticNoteY(0))-25,850,55);
    const auto closedEnvelope=[&](const juce::Image& image){
        const auto area=imageArea.getIntersection(image.getBounds());int left=850,right=0,top=image.getHeight(),bottom=0;
        const auto green=[&](int x,int y){const auto c=image.getPixelAt(x,y);return c.getGreen()>180&&c.getRed()<150&&c.getBlue()<210;};
        for(int y=area.getY();y<area.getBottom();++y)for(int x=0;x<850;++x)if(green(x,y))
        {left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}
        if(right-left<500||bottom-top<15)return false;
        const auto vertical=[&](int edge){int best=0;for(int x=std::max(0,edge-3);x<=std::min(849,edge+3);++x)
        {int count=0;for(int y=top;y<=bottom;++y)count+=green(x,y);best=std::max(best,count);}return best;};
        const auto horizontal=[&](int edge){int best=0;for(int y=std::max(0,edge-3);y<=std::min(image.getHeight()-1,edge+3);++y)
        {int count=0;for(int x=left;x<=right;++x)count+=green(x,y);best=std::max(best,count);}return best;};
        return vertical(left)>bottom-top-5&&vertical(right)>bottom-top-5
            &&horizontal(top)>(right-left)*.8&&horizontal(bottom)>(right-left)*.8;};
    const auto unified=syllableCapture();
    check("unity_envelope_has_four_visible_edges_without_note_range",closedEnvelope(unified));
    check("single_syllable_preview_written",saveImage("single-syllable-closed-envelope",unified.getClippedImage(imageArea)));
    auto withoutSegments=syllableData;withoutSegments.tracks[0].clips[0].notes[0].nativeSegments.clear();
    syllableModel.replace(withoutSegments);syllableModel.dispatchPendingMessages();syllableRoll.diagnosticRefresh();
    check("same_alias_cv_has_one_label_and_no_false_divider",difference(unified,syllableCapture(),imageArea)==0);
    syllableModel.replace(syllableData);syllableModel.dispatchPendingMessages();syllableRoll.diagnosticRefresh();
    syllableRoll.setTool(PianoRollComponent::Tool::amplitude);
    check("amplitude_tool_also_closes_unity_envelope",closedEnvelope(syllableCapture()));
    check("closed_outline_does_not_write_silent_edge_points",syllableModel.snapshot().tracks[0].clips[0].notes[0].amplitudeEnvelope.empty());
    syllableRoll.setTool(PianoRollComponent::Tool::note);syllableRoll.setSourceEditMode(true);
    check("source_edit_retains_cv_details",difference(unified,syllableCapture(),imageArea)>20);
    syllableRoll.setSourceEditMode(false);
    auto distinctSegments=syllableData;distinctSegments.tracks[0].clips[0].notes[0].nativeSegments[0].alias="b";
    distinctSegments.tracks[0].clips[0].notes[0].nativeSegments[1].alias="aip";
    syllableModel.replace(distinctSegments);syllableModel.dispatchPendingMessages();syllableRoll.diagnosticRefresh();
    check("meaningful_subdivision_labels_remain_visible",difference(unified,syllableCapture(),imageArea)>20
        &&syllableModel.snapshot().tracks[0].clips[0].notes[0].nativeSegments.size()==2);
    check("distinct_segments_preview_written",saveImage("distinct-syllable-segments",syllableCapture().getClippedImage(imageArea)));
    std::cout<<"native_source_pitch_ok="<<ok<<"; checks="<<checks<<std::endl;return ok;
}
}
