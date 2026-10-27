#pragma once
#include "../ProjectModel.h"
#include "../OttoMaterialIO.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
namespace hachi
{
// Model-authority smoke: synthetic UST/USTX-like score owners and real WAV rows;
// no ASR, no renderer oracle, no fake second replacement writer.
inline bool runNativeScoreMaterialReplacementSmoke(const juce::File& output)
{
    const auto root=output.getChildFile("native-score-material-replacement-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+(pass||error.isEmpty()?juce::String{}:" | "+error)+"\n";};
    const auto source=root.getChildFile("full.wav");{juce::AudioBuffer<float> pcm(1,96000);for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,.12f*std::sin(juce::MathConstants<double>::twoPi*173*i/48000.0));auto stream=source.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.release(),48000,1,16,{},0):nullptr);check("full_source_fixture",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));}
    SampleRegionSetting da;da.materialAudioId="variant";da.materialRegionId="da";da.name="da";da.regionStartSeconds=0;da.regionEndSeconds=.8;da.alignmentSeconds=.35;da.fixedDurationSeconds=.2;da.overlapSeconds=.03;da.nativeUnpitched=true;da.role=NativeSegmentRole::vowel;da.connectedNextRegionId="re";
    SampleRegionSetting re=da;re.materialRegionId="re";re.name="re";re.regionStartSeconds=.8;re.regionEndSeconds=1.45;re.alignmentSeconds=1.05;re.fixedDurationSeconds=.15;re.connectedNextRegionId="ga";
    SampleRegionSetting ga=da;ga.materialRegionId="ga";ga.name="ga";ga.regionStartSeconds=1.45;ga.regionEndSeconds=2.0;ga.alignmentSeconds=1.7;ga.fixedDurationSeconds=.15;ga.connectedNextRegionId.clear();
    auto rows=std::make_shared<const std::vector<SampleRegionSetting>>(std::vector<SampleRegionSetting>{da,re,ga});OttoMaterialIO::Document doc;doc.audio={{"variant",source.getFileName(),*rows}};const auto manifest=root.getChildFile("otto.ini");check("full_variant_manifest",OttoMaterialIO::saveDocument(manifest,doc,error));
    ProjectData data;data.bpm=137;TrackData track;track.id="ust-score";track.trackType=TrackType::hachi;track.compose=true;ClipData clip;clip.id="score-owner";clip.sourceFile=root.getChildFile("score.ustx");clip.durationSeconds=2.4;clip.sourceDurationSeconds=2.4;clip.sourceTimeMap={{0,0},{2.4,2.4}};
    auto note=[&](const SampleRegionSetting& row,const juce::String& id,double start,double duration){NoteData n;n.id=id;n.label=row.name;n.startSeconds=start;n.durationSeconds=duration;n.midiNote=60;n.sourceMidiCenter=60;n.nativeUnpitched=true;n.materialSourceStartSeconds=n.nativeSourceStartSeconds=row.regionStartSeconds;n.materialSourceEndSeconds=n.nativeSourceEndSeconds=row.regionEndSeconds;n.materialAudioId=row.materialAudioId;n.materialRegionId=row.materialRegionId;n.materialBoundAlias=row.name;n.materialAliasFollowsPrimary=n.materialLabelFollowsAlias=true;n.nativeSegments=SampleSettings::nativeSegmentsFor(row);n.nativeRole=row.role;n.utauPreutteranceOverrideEnabled=true;n.utauPreutteranceSeconds=row.alignmentSeconds-row.regionStartSeconds;n.utauConsonantVelocity=100;n.nativeScoreMaterialAligned=true;return n;};
    clip.notes={note(da,"score-da",0,.8),note(re,"score-re",.8,.7),note(ga,"score-ga",1.5,.7)};track.clips={clip};data.tracks={track};ProjectModel model;model.resetDocument(data);const auto before=model.contentFingerprint();const auto revision=model.revisionNumber();
    std::vector<ProjectModel::MaterialSourceChoice> choices{{"score-da",source,rows,"variant","da","da"},{"score-re",source,rows,"variant","re","re"},{"score-ga",source,rows,"variant","ga","ga"}};
    const auto preview=model.replaceNotesMaterialSources(choices,revision,error,false);check("batch_preview_no_mutation",!preview.rejected&&preview.changed&&model.contentFingerprint()==before&&model.revisionNumber()==revision&&!model.canUndo());
    const auto applied=model.replaceNotesMaterialSources(choices,revision,error,true);const auto after=model.snapshot();check("batch_one_atomic_undo",!applied.rejected&&applied.changed&&model.revisionNumber()==revision+1&&model.canUndo());
    bool identities=true,controls=true,owners=true;for(const auto& n:after.tracks[0].clips[0].notes){identities&=n.id.startsWith("score-")&&n.midiNote==60&&n.nativeScoreMaterialAligned;controls&=n.durationSeconds>0&&n.materialSourceStartSeconds>=0;owners&=n.clipPartId.isNotEmpty();}check("score_ids_controls_and_explicit_owners_preserved",identities&&controls&&owners);
    check("batch_undo_restores_score_exactly",model.undo()&&model.contentFingerprint()==before&&!model.canUndo());
    const auto current=model.revisionNumber(); // undo itself advances the revision counter
    const auto stale=model.replaceNotesMaterialSources(choices,current+99,error,false);check("stale_revision_rejected_atomically",stale.rejected&&model.contentFingerprint()==before&&model.revisionNumber()==current);
    error.clear();
    auto ambiguous=choices;ambiguous[1].audioId="other-variant";const auto rejected=model.replaceNotesMaterialSources(ambiguous,current,error,false);check("mixed_variant_rejected_without_mutation",rejected.rejected&&model.contentFingerprint()==before);
    const auto sourceHash=juce::SHA256(source).toHexString(),manifestHash=juce::SHA256(manifest).toHexString();const auto saved=root.getChildFile("score.hjpx");ProjectModel cold;check("cold_score_roundtrip",model.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().tracks[0].clips[0].notes.size()==3);
    check("source_PCM_and_manifest_unchanged",juce::SHA256(source).toHexString()==sourceHash&&juce::SHA256(manifest).toHexString()==manifestHash);
    const auto written=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_score_material_replacement="<<ok<<"|fixture="<<root.getFullPathName()<<std::endl;if(!ok)std::cerr<<log<<std::endl;return ok&&written;
}
}
