#pragma once
#include "../ProjectModel.h"
#include "../NativeNoteTiming.h"
#include "../NativeTrimSource.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
namespace hachi
{
inline bool runNativeSourceTrendPitchSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("source-trend-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool value){ok&=value;log+=juce::String(name)+"="+(value?"PASS":"FAIL")+(error.isEmpty()?juce::String{}:" | "+error)+"\n";std::cout<<name<<"="<<value<<std::endl;error.clear();};
    const auto wave=root.getChildFile("control.wav");{juce::AudioBuffer<float> pcm(1,48000);pcm.clear();juce::WavAudioFormat format;auto stream=wave.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,16,{},0));check("control_wave_header",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));}
    SampleRegionSetting row;row.materialAudioId="voice";row.materialRegionId="a";row.name="a";row.regionEndSeconds=1;row.alignmentSeconds=.1;row.fixedDurationSeconds=.15;row.role=NativeSegmentRole::vowel;auto rows=std::make_shared<const std::vector<SampleRegionSetting>>(std::vector<SampleRegionSetting>{row});
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;ClipData score;score.id="score";score.sourceFile=root.getChildFile("score.ust");score.durationSeconds=score.sourceDurationSeconds=2;NoteData n;n.id="target";n.label="a";n.startSeconds=.5;n.durationSeconds=1.5;n.midiNote=64;n.utauPreutteranceOverrideEnabled=true;n.utauPreutteranceSeconds=.1;n.contour={{0,0,0,true},{1.5,0,0,true}};n.pitchControlPoints={{0,64},{.6,65},{1.5,64}};n.vibratoEnabled=true;n.modulation=.25;score.notes={n};track.clips={score};seed.tracks={track};ProjectModel model;model.resetDocument(seed);
    auto report=model.replaceNotesMaterialSources({{"target",wave,rows,"voice","a","a"}},model.revisionNumber(),error);check("source_binding_requests_exact_analysis",!report.rejected&&report.changed&&report.tickets.size()==1);
    auto pending=model.snapshot().tracks[0].clips[0].notes[0];check("protocol_UTAU_curve_not_used_as_native_target",pending.nativeSourceTrendTranspose&&pending.pitchControlPoints.empty()&&pending.contour.empty()&&!pending.vibratoEnabled&&pending.drift==1&&pending.modulation==1&&pending.midiNote==64);
    // Deliberately controlled model facts, not a detector/NN accuracy claim.
    NoteData facts;facts.id="controlled-source";facts.durationSeconds=1;facts.midiNote=facts.sourceMidiCenter=60;facts.sourcePitchMeasured=true;facts.contour={{0,-30,-15,true},{.25,35,20,true},{.5,-10,-5,true},{.75,22,12,true},{1,-15,-8,true}};
    if(report.tickets.empty())return false;check("controlled_facts_complete_source_binding",model.completeMaterialSourceAnalysis(report.tickets[0],{facts},error));
    const auto data=model.snapshot();const auto& parent=data.tracks[0].clips[0];const auto& live=parent.notes[0];const ClipData* owner=nullptr;for(const auto& part:parent.parts)if(part.id==live.clipPartId)owner=&part;
    bool transposed=owner&&!live.contour.empty()&&live.sourcePitchMeasured&&live.sourceMidiCenter>=0;float lo=1.e9f,hi=-1.e9f;
    if(owner){const auto clock=nativeClipClock(*owner);for(const auto& p:live.contour)if(p.voiced){const auto source=owner->sourceOffsetSeconds+nativeSourceTimeAt(clock,live.startSeconds-owner->startSeconds+p.timeSeconds);const auto ref=nativeReferencePitchAt(*owner,source);const auto heard=live.midiNote+renderedPitchCents(live,p)/100;transposed&=ref&&!p.hasManualTarget&&std::abs(heard-(64+ref->midi-live.sourceMidiCenter))<1.e-4;lo=std::min(lo,heard);hi=std::max(hi,heard);}}
    check("nonlinear_source_clock_transposes_detail_without_flattening",transposed&&hi-lo>.3f&&live.midiNote==64);
    const auto file=root.getChildFile("natural.hjpx");ProjectModel cold;check("natural_pitch_policy_and_detail_survive_cold_open",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().tracks[0].clips[0].notes[0].nativeSourceTrendTranspose&&cold.snapshot().tracks[0].clips[0].notes[0].pitchControlPoints.empty()&&cold.snapshot().tracks[0].clips[0].notes[0].contour.size()==live.contour.size());
    model.setNotePitchCurve("target",{{0,64.3f},{1.5,64.6f}},true,true);auto edited=model.snapshot().tracks[0].clips[0].notes[0];auto again=model.replaceNotesMaterialSources({{"target",wave,rows,"voice","a","a"}},model.revisionNumber(),error);bool explicitKept=!again.rejected;for(const auto& ticket:again.tickets)explicitKept&=model.completeMaterialSourceAnalysis(ticket,{facts},error);const auto finalNote=model.snapshot().tracks[0].clips[0].notes[0];
    check("later_explicit_native_edits_not_replaced_by_new_analysis",explicitKept&&finalNote.pitchControlPoints.size()==edited.pitchControlPoints.size()&&!finalNote.pitchControlPoints.empty()&&std::abs(finalNote.pitchControlPoints[0].targetMidi-64.3f)<1.e-5);
    root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_source_trend_fixture="<<root.getFullPathName()<<std::endl;return ok;
}
}
