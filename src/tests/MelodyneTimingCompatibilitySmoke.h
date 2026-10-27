#pragma once
#include "../backend/MelodyneImporter.h"
#include "../NativeNoteTiming.h"
#include "../NativeMaterialContinuity.h"
#include "../NativeAudioOverlap.h"
#include "TeioProjectMaterialRegistrationAudioGate.h"
#include "../I18n.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <filesystem>

namespace hachi
{
// Real MPD/MLD import + metadata/request compatibility; not an acoustic test.
// Only referenced recordings are copied into this isolated output directory.
inline bool runMelodyneTimingCompatibilitySmoke(const juce::File& output,const juce::File& actualMelodyne,
                                                const juce::File& legacyHjpx={})
{
    const auto root=output.getChildFile("melodyne-timing-"+juce::Uuid().toString());if(root.createDirectory().failed())return false;
    bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool value){ok&=value;log+=juce::String(name)+"="+(value?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<value<<"|"<<error<<std::endl;error.clear();};
    const auto finish=[&]{std::cout<<"melodyne_timing_fixture="<<root.getFullPathName()<<std::endl;return root.getChildFile("melodyne-timing.txt").replaceWithText(log)&&ok;};
    const auto sameClock=[](const std::vector<SourceTimePoint>& a,const std::vector<SourceTimePoint>& b)
    {if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].targetSeconds!=b[i].targetSeconds||a[i].sourceSeconds!=b[i].sourceSeconds)return false;return true;};
    const auto aligned=[](const ProjectData& data)
    {
        for(const auto& track:data.tracks)for(const auto& clip:track.clips)
        {
            const auto clock=nativeClipClock(clip);
            for(const auto& note:clip.notes)
            {
                const auto row=nativeNoteSourceTiming(note,clip,clock);
                const auto sourceC=clip.sourceOffsetSeconds+nativeSourceTimeAt(clock,note.startSeconds+note.consonantSeconds);
                if(std::abs(sourceC-row.alignmentSeconds)>1.e-8)return false;
            }
        }
        return true;
    };
    if(!actualMelodyne.existsAsFile()){error="Supply an actual Melodyne .mpd/.mld project";check("actual_Melodyne_required",false);return finish();}
    const auto projectSha=juce::SHA256(actualMelodyne).toHexString();
    auto imported=backend::MelodyneImporter::importProject(actualMelodyne,error);
    check("actual_fresh_Melodyne_import",imported.has_value());if(!imported)return finish();
    check("actual_import_resolved_all_sources",imported->missingFiles.isEmpty()&&!imported->project.tracks.empty());
    check("fresh_import_C_matches_effective_P",aligned(imported->project));
    const auto media=root.getChildFile("used-recordings");media.createDirectory();
    std::map<juce::String,std::pair<juce::File,juce::String>> originals;
    std::map<juce::String,juce::File> copies;
    for(auto& track:imported->project.tracks)for(auto& clip:track.clips)
    {
        const auto path=clip.sourceFile.getFullPathName();
        if(!clip.sourceFile.existsAsFile()){check("resolved_physical_source_exists",false);return finish();}
        if(!copies.contains(path))
        {
            const auto copy=media.getChildFile(juce::String(int(copies.size()))+"-"+clip.sourceFile.getFileName());
            // Put manifests beside isolated names without copying large media
            // payloads. A hard link is byte-identical and source is read-only
            // throughout this smoke; fall back to a physical copy only when
            // the target filesystem cannot link across the source volume.
            std::error_code linkError;
            std::filesystem::create_hard_link(std::filesystem::path(clip.sourceFile.getFullPathName().toWideCharPointer()),
                std::filesystem::path(copy.getFullPathName().toWideCharPointer()),linkError);
            const auto isolated=linkError?clip.sourceFile.copyFileTo(copy):true;
            check("isolate_sidecar_without_mutating_source",isolated);
            originals[path]={clip.sourceFile,juce::SHA256(clip.sourceFile).toHexString()};copies[path]=copy;
        }
        clip.sourceFile=copies.at(path);
    }
    // The same post-import track configuration the GUI applies (NSF default).
    for(auto& track:imported->project.tracks)
    {track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.nativeNsfAudio=true;track.trackType=TrackType::hachi;
     track.stretchAlgorithm=StretchAlgorithm::melodyneHybrid;track.renderOrder=RenderOrder::stretchSpliceThenPitch;track.compose=true;}
    const auto before=imported->project;
    std::cout<<"phase=before-request-fingerprint begin clips="<<copies.size()<<std::endl;
    const auto requestBefore=teioRegistrationAudioDetail::requestFingerprint(before);
    std::cout<<"phase=before-request-fingerprint done; conversion begin"<<std::endl;
    // Production import entries convert in memory (publishAnnotations=false);
    // publishing 98 sidecars is a separate explicit registration action.
    juce::StringArray warnings;const bool converted=SampleSettings::convertMelodyneProject(imported->project,warnings,true,false);
    std::cout<<"phase=conversion done"<<std::endl;
    check("actual_import_source_annotation_conversion",converted);if(!converted)return finish();
    check("post_conversion_C_matches_effective_physical_P",aligned(imported->project));
    {
        // The derived P is canonical native geometry, never a UTAU-only override warning.
        const auto warnings=AudioEngine::renderCapabilityWarnings(imported->project).joinIntoString("\n");
        check("converted_P_is_not_reported_as_UTAU_only_override",!warnings.contains(I18n::uiText("engine.timingOverrideUtauOnly")));
    }
    check("actual_fresh_import_acoustic_request_vectors_exact",teioRegistrationAudioDetail::requestFingerprint(imported->project)==requestBefore);
    bool clocks=true,knots=true,fades=true,contours=true;
    for(std::size_t t=0;t<before.tracks.size();++t)for(std::size_t c=0;c<before.tracks[t].clips.size();++c)
    {
        const auto& old=before.tracks[t].clips[c];const auto& now=imported->project.tracks[t].clips[c];
        clocks &= sameClock(nativeClipClock(old),nativeClipClock(now));
        if(!old.sourceTimeMap.empty())knots &= sameClock(old.sourceTimeMap,now.sourceTimeMap);
        fades &= old.fadeInSeconds==now.fadeInSeconds&&old.fadeOutSeconds==now.fadeOutSeconds&&old.crossfadeInSeconds==now.crossfadeInSeconds
            &&old.crossfadeOutSeconds==now.crossfadeOutSeconds&&old.gain==now.gain;
        for(std::size_t n=0;n<old.notes.size();++n)
        {
            const auto& a=old.notes[n];const auto& b=now.notes[n];
            contours &= a.midiNote==b.midiNote&&a.sourceMidiCenter==b.sourceMidiCenter&&a.drift==b.drift&&a.modulation==b.modulation
                &&a.formantSemitones==b.formantSemitones&&a.breath==b.breath&&a.gain==b.gain&&a.attackSpeed==b.attackSpeed&&a.contour.size()==b.contour.size();
            for(std::size_t p=0;p<a.contour.size()&&p<b.contour.size();++p)
                contours &= a.contour[p].timeSeconds==b.contour[p].timeSeconds&&a.contour[p].relativeCents==b.contour[p].relativeCents
                    &&a.contour[p].withoutVibratoCents==b.contour[p].withoutVibratoCents&&a.contour[p].hasManualTarget==b.contour[p].hasManualTarget
                    &&a.contour[p].manualTargetCents==b.contour[p].manualTargetCents;
        }
    }
    // Visible lead-in: P derived from the imported voiced evidence, never a
    // guessed body. Overlap policy only flips the editing flag.
    int leads=0,notes=0,unvoicedStarts=0,overlapTracks=0,flaggedOverlap=0,attacks=0;
    double attackTotal=0,attackMin=1e9,attackMax=0;
    for(const auto& track:imported->project.tracks)
    {
        if(nativeAudioOverlapCount(track)>0){++overlapTracks;flaggedOverlap+=track.allowNativeAudioOverlap?1:0;}
        for(const auto& clip:track.clips)for(const auto& note:clip.notes)
        {
            ++notes;const auto span=nativeNoteVisualSpan(note,clip);if(span.p>span.from+1.e-4)++leads;
            if(!note.contour.empty()&&!note.contour.front().voiced)++unvoicedStarts;
            if(note.melodyneAttackSeconds>1.e-9){++attacks;attackTotal+=note.melodyneAttackSeconds;
                attackMin=std::min(attackMin,note.melodyneAttackSeconds);attackMax=std::max(attackMax,note.melodyneAttackSeconds);}
        }
    }
    log+="notes="+juce::String(notes)+" visible_lead_ins="+juce::String(leads)+" unvoiced_onsets="+juce::String(unvoicedStarts)
        +" melodyne_attack_durations="+juce::String(attacks)+" overlapping_tracks="+juce::String(overlapTracks)+"\n";
    if(attacks>0)log+="attack_ms_avg="+juce::String(attackTotal/attacks*1000,1)+" min="+juce::String(attackMin*1000,1)+" max="+juce::String(attackMax*1000,1)+"\n";
    std::cout<<"notes="<<notes<<" visible_lead_ins="<<leads<<" unvoiced_onsets="<<unvoicedStarts
        <<" melodyne_attack_durations="<<attacks<<" overlapping_tracks="<<overlapTracks<<std::endl;
    check("Melodyne_onsets_with_unvoiced_evidence_show_lead_in",unvoicedStarts==0||leads>0);
    check("overlapping_Melodyne_tracks_allow_overlap_editing",flaggedOverlap==overlapTracks);
    {
        // Same-exe render pair: the raw import (old P=onset request) and the
        // converted project must produce identical audio. See CLI render.
        ProjectModel raw,converted;raw.resetDocument(before);converted.resetDocument(imported->project);
        check("render_pair_saved",raw.save(root.getChildFile("raw-import.hjpx"),error)&&converted.save(root.getChildFile("converted.hjpx"),error));
    }
    check("full_effective_source_clocks_exact",clocks);check("authoritative_Melodyne_time_function_knots_exact",knots);
    check("author_fades_and_clip_gain_unchanged",fades);check("original_voiced_contours_and_musical_controls_unchanged",contours);
    // In-memory import: nothing is published beside the recordings, and every
    // project note row keeps C == P (checked above via the effective timing).
    bool unpublished=true;const auto phaseStart=juce::Time::getMillisecondCounterHiRes();
    std::cout<<"phase=unpublished-check begin"<<std::endl;
    for(const auto& item:copies)unpublished&=!SampleSettings::hasNativeAnnotation(item.second,&error);
    std::cout<<"phase=unpublished-check done ms="<<(juce::Time::getMillisecondCounterHiRes()-phaseStart)<<std::endl;
    error.clear();
    check("in_memory_import_publishes_no_source_annotations",unpublished);
    std::cout<<"phase=original-sha begin"<<std::endl;
    for(const auto& item:originals)check("original_physical_recording_read_only",juce::SHA256(item.second.first).toHexString()==item.second.second);
    std::cout<<"phase=original-sha done; nonzero fixture begin"<<std::endl;
    check("original_Melodyne_project_read_only",juce::SHA256(actualMelodyne).toHexString()==projectSha);
    // Focused nonzero-P metadata case uses an actual copied recording and an
    // explicit non-linear clock. It is not substituted for the real MPD gate.
    if(!copies.empty())
    {
        const auto isolated=root.getChildFile("nonzero-marker");isolated.createDirectory();const auto source=isolated.getChildFile(copies.begin()->second.getFileName());
        check("nonzero_marker_uses_actual_audio_copy",copies.begin()->second.copyFileTo(source));
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
        if(reader)
        {
            const auto duration=std::min(.5,reader->lengthInSamples/reader->sampleRate);ProjectData fixture;TrackData track;track.id="nonzero-P";
            track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.nativeNsfAudio=true;
            ClipData clip;clip.id="nonzero-P-clip";clip.sourceFile=source;clip.durationSeconds=clip.sourceDurationSeconds=duration;
            clip.sourceTimeMap={{0,0},{duration*.5,duration*.3},{duration,duration}};
            NoteData note;note.id="nonzero-P-note";note.durationSeconds=duration;note.midiNote=64;note.sourceMidiCenter=60;note.sourcePitchMeasured=true;
            note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=duration*.25;note.consonantSeconds=duration*.1;
            note.contour={{0,0,0,true},{duration,0,0,true}};clip.notes={note};track.clips={clip};fixture.tracks={track};
            const auto oldClock=nativeClipClock(clip);const auto oldP=nativeNoteSourceTiming(note,clip,oldClock).alignmentSeconds;
            warnings.clear();check("nonzero_effective_marker_conversion",SampleSettings::convertMelodyneProject(fixture,warnings));
            const auto& after=fixture.tracks[0].clips[0];const auto timing=nativeNoteSourceTiming(after.notes[0],after,nativeClipClock(after));
            check("nonzero_effective_source_P_preserved_C_projected_via_old_clock",std::abs(timing.alignmentSeconds-oldP)<1.e-8&&aligned(fixture)&&sameClock(oldClock,nativeClipClock(after)));
        }
        else check("nonzero_marker_recording_header",false);
    }
    if(legacyHjpx.existsAsFile())
    {
        const auto sha=juce::SHA256(legacyHjpx).toHexString();ProjectModel legacy;check("old_HJPX_read_without_new_default_migration",legacy.load(legacyHjpx,error));
        const auto old=legacy.snapshot();const auto fp=teioRegistrationAudioDetail::requestFingerprint(old);const auto saved=root.getChildFile("old-project-copy.hjpx");ProjectModel cold;
        check("old_HJPX_roundtrip_retains_exact_native_request",legacy.save(saved,error)&&cold.load(saved,error)&&teioRegistrationAudioDetail::requestFingerprint(cold.snapshot())==fp);
        check("old_HJPX_bytes_unchanged",juce::SHA256(legacyHjpx).toHexString()==sha);
    }
    log+="SCOPE=actual fresh Melodyne import/source metadata and request compatibility; actual waveform equality is the separate real teio registration gate.\n";
    return finish();
}
}
