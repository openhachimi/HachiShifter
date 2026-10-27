#pragma once
#include "../PronunciationTiming.h"
#include "../OttoMaterialIO.h"
#include "../MaterialPronunciationCatalog.h"
#include "../ProjectModel.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
namespace hachi
{
inline bool runNativeTransitionShapeSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("transition-shape-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool value){ok&=value;log+=juce::String(name)+"="+(value?"PASS":"FAIL")+(error.isEmpty()?juce::String{}:" | "+error)+"\n";std::cout<<name<<"="<<value<<std::endl;error.clear();};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-8;};
    const auto wave=root.getChildFile("source.wav");{juce::AudioBuffer<float> pcm(1,19200);pcm.clear();juce::WavAudioFormat format;auto stream=wave.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),48000,1,16,{},0));check("control_source_header",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));}
    SampleRegionSetting row;row.materialAudioId="voice";row.materialRegionId="eg";row.name="e_g";row.regionStartSeconds=.1;row.regionEndSeconds=.19;row.alignmentSeconds=.14;row.fixedDurationSeconds=.03;row.role=NativeSegmentRole::vowel;row.pronunciationKind=PronunciationKind::transition;
    check("manual_middle_boundary_valid",editNativeTransitionShape(row,.145,error));
    check("transition_timing_has_no_C_or_P",applyPronunciationTiming(row,pronunciationTimingFor(row),error)&&near(row.alignmentSeconds,.1)&&near(row.fixedDurationSeconds,0)&&row.role==NativeSegmentRole::transition);
    const auto segments=pronunciationSegmentsFor(row);check("one_middle_cut_two_non_CV_elastic_sides",segments.size()==2&&segments[0].role==NativeSegmentRole::transition&&segments[1].role==NativeSegmentRole::transition&&segments[0].stretchable&&segments[1].stretchable&&near(segments[0].sourceEndSeconds,.045)&&near(segments[1].sourceStartSeconds,.045)&&near(segments[0].alignmentSeconds,0));
    std::vector<SourceTimePoint> map;check("transition_stretches_without_fake_protected_consonant",makePronunciationStretchMap(row,.13,map,error)&&!map.empty()&&near(map.front().targetSeconds,0)&&near(map.back().targetSeconds,.13));
    NativePronunciationEdges edges;edges.entryFromSourceSeconds=.1;edges.entryToSourceSeconds=.11;edges.exitFromSourceSeconds=.18;edges.exitToSourceSeconds=edges.terminationSourceSeconds=.19;edges.provenance="manual-left-right-overlap";row.nativePronunciationEdges=edges;
    OttoMaterialIO::Document document;document.audio={{"voice",wave.getFileName(),{row}}};const auto manifest=root.getChildFile("otto.ini");check("OTTO_shape_roundtrip_write",OttoMaterialIO::saveDocument(manifest,document,error));OttoMaterialIO::Document restored;check("OTTO_shape_roundtrip_exact",OttoMaterialIO::loadDocument(manifest,restored,error)&&restored.audio.size()==1&&restored.audio[0].rows.size()==1&&restored.audio[0].rows[0].nativeTransition==row.nativeTransition&&restored.audio[0].rows[0].nativePronunciationEdges==row.nativePronunciationEdges);
    auto invalid=row;const auto before=invalid.nativeTransition;check("outside_middle_rejected_without_mutation",!editNativeTransitionShape(invalid,.2,error)&&invalid.nativeTransition==before);
    NativeTransitionShape shape;juce::Array<juce::var> bad;bad.add(2);bad.add(.145);bad.add("future");check("future_shape_version_refused",!nativeTransitionShapeFromValue(juce::var(bad),shape,error));
    auto left=row;left.materialRegionId="left";left.pronunciationKind=PronunciationKind::normal;left.regionStartSeconds=0;left.regionEndSeconds=.1;left.alignmentSeconds=.02;left.nativeTransition.reset();left.nativePronunciationEdges.reset();left.connectedNextRegionId="eg";
    check("transition_never_a_partial_source_continuity_chain",!MaterialPronunciationCatalog::continuousRows({left,row},"left","eg",error));
    ProjectData data;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;ClipData clip;clip.id="owner";clip.sourceFile=wave;clip.sourceOffsetSeconds=.1;clip.durationSeconds=clip.sourceDurationSeconds=.09;NoteData note;note.id="bridge";note.label="e_g";note.durationSeconds=.09;note.materialAudioId="voice";note.materialRegionId="eg";note.nativeSourceStartSeconds=note.materialSourceStartSeconds=.1;note.nativeSourceEndSeconds=note.materialSourceEndSeconds=.19;note.pronunciationKind=PronunciationKind::transition;note.nativeTransition=row.nativeTransition;note.nativePronunciationEdges=row.nativePronunciationEdges;note.nativeSegments=segments;clip.notes={note};track.clips={clip};data.tracks={track};ProjectModel model,cold;model.resetDocument(data);const auto file=root.getChildFile("transition.hjpx");check("HJPX_transition_shape_and_edges_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().tracks[0].clips[0].notes[0].nativeTransition==note.nativeTransition&&cold.snapshot().tracks[0].clips[0].notes[0].nativePronunciationEdges==note.nativePronunciationEdges);
    root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"transition_shape_fixture="<<root.getFullPathName()<<std::endl;return ok;
}
}
