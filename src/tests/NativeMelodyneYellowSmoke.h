#pragma once
#include "../PianoRollComponent.h"
#include "../SampleSettings.h"
#include "../NativeMaterialContinuity.h"
#include "../backend/MelodyneImporter.h"
#include "../Theme.h"
#include "TeioProjectMaterialRegistrationAudioGate.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>

namespace hachi
{
// Actual fresh import, no sidecar writes, synthesis, reanalysis or guessed phonemes.
inline bool runNativeMelodyneYellowSmoke(const juce::File& output,const juce::File& mpd)
{
    if(output.createDirectory().failed())return false;
    bool ok=true;juce::String error,log;
    const auto check=[&](const char* key,bool pass){ok&=pass;log+=juce::String(key)+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<key<<"="<<pass<<std::endl;};
    const auto sha=juce::SHA256(mpd).toHexString();
    auto imported=backend::MelodyneImporter::importProject(mpd,error);
    check("real_MPD_import",imported.has_value());if(!imported)return false;
    const auto raw=imported->project;juce::StringArray warnings;
    auto oldAnnotation=raw;
    for(auto& tr:oldAnnotation.tracks)for(auto& c:tr.clips)for(auto& n:c.notes)
    {n.utauPreutteranceOverrideEnabled=false;n.utauPreutteranceSeconds=0.0;n.consonantSeconds=0.0;}
    const auto priorRequest=teioRegistrationAudioDetail::requestFingerprint(oldAnnotation);
    check("explicit_anchor_annotations_preserve_pre_fix_acoustic_request",teioRegistrationAudioDetail::requestFingerprint(raw)==priorRequest);
    check("in_memory_conversion_only",SampleSettings::convertMelodyneProject(imported->project,warnings,true,false));
    check("anchor_conversion_preserves_pre_fix_acoustic_request",teioRegistrationAudioDetail::requestFingerprint(imported->project)==priorRequest);
    int notes=0,positiveP=0,stableLater=0,noStable=0,voicedFromStart=0;
    int positiveAnchors=0,zeroAnchors=0,missingAnchors=0,ordinaryVoicedLeads=0;
    bool clocks=true,uv=true,pEvidence=true,sourceUnits=true,consonantP=true;
    // Raw graph anchor provenance is the oracle, never the contour's voiced
    // index. Keep the former stable-onset statistics only as diagnostics.
    const auto& anchors=imported->alignmentEvidence;
    I18n strings;strings.setLanguage(I18n::Language::enUS);
    Palette::applyTheme("dark",juce::Colour(0xff7f69ca),juce::Colour(0xffcbcbfa),juce::Colour(0xfff4c000));
    for(std::size_t t=0;t<imported->project.tracks.size();++t)
    for(std::size_t c=0;c<imported->project.tracks[t].clips.size();++c)
    {
        const auto& clip=imported->project.tracks[t].clips[c];const auto& old=raw.tracks[t].clips[c];
        const auto clock=nativeClipClock(clip),beforeClock=nativeClipClock(old);
        clocks&=clock.size()==beforeClock.size();for(std::size_t i=0;i<std::min(clock.size(),beforeClock.size());++i)clocks&=clock[i].targetSeconds==beforeClock[i].targetSeconds&&clock[i].sourceSeconds==beforeClock[i].sourceSeconds;
        for(std::size_t n=0;n<clip.notes.size();++n)
        {
            const auto& note=clip.notes[n];++notes;const auto visual=nativeNoteVisualSpan(note,clip,&clock);
            int stable=-1;for(std::size_t i=0;i+1<note.contour.size();++i)if(note.contour[i].voiced&&note.contour[i+1].voiced){stable=int(i);break;}
            if(stable<0)++noStable;else if(stable==0)++voicedFromStart;else ++stableLater;
            const auto anchor=std::find_if(anchors.begin(),anchors.end(),[&](const auto& a){return a.noteId==note.id;});
            if(anchor==anchors.end())++missingAnchors;
            else if(anchor->targetSeconds>1.e-9)++positiveAnchors;else ++zeroAnchors;
            if(!visual.transition&&!note.melodyneConsonantCandidate)
            {
                const auto expectedSource=anchor==anchors.end()?clip.sourceOffsetSeconds:anchor->sourceSeconds;
                const auto expected=clip.startSeconds+nativeTargetTimeAt(clock,expectedSource-clip.sourceOffsetSeconds);
                pEvidence&=std::abs(visual.p-expected)<1.e-7;
                if(anchor!=anchors.end())
                    sourceUnits&=std::abs(note.utauPreutteranceSeconds-(anchor->sourceSeconds-anchor->supportSourceSeconds))<1.e-7;
                consonantP&=std::abs(note.consonantSeconds-(expected-visual.scoreFrom))<1.e-7;
                if(stable==0&&visual.p>visual.from+1.e-6)++ordinaryVoicedLeads;
            }
            uv&=note.contour.size()==old.notes[n].contour.size();for(std::size_t i=0;i<std::min(note.contour.size(),old.notes[n].contour.size());++i)uv&=note.contour[i].voiced==old.notes[n].contour[i].voiced;
            log+="note="+note.id+" source="+clip.sourceFile.getFileName()+" nominal="+juce::String(visual.scoreFrom,9)+" support="+juce::String(visual.from,9)+" clockP="+juce::String(visual.p,9)+" stableIndex="+juce::String(stable)+" typed="+juce::String(visual.transition?1:0)+"\n";
            if(visual.transition||visual.p<=visual.from+1.e-6)continue;
            ++positiveP;
            // Isolate the real physical element for pixel checks: no other note can
            // hide it by z-order. Only viewport position is shifted, never source clock.
            ProjectData view;auto tr=imported->project.tracks[t];tr.clips={clip};tr.clips[0].notes={note};tr.clips[0].startSeconds=.3-note.startSeconds;view.tracks={tr};
            ProjectModel model;model.resetDocument(view);PianoRollComponent roll(model,strings);
            roll.setReadsVoicebankInBackground(false);roll.diagnosticSetCulling(false);roll.setFocusedTrack(tr.id);
            roll.setPixelsPerSecond(1400);roll.setRowHeight(32);roll.setSize(1100,4400);roll.setShowEnvelope(true);roll.setShowPitchLine(false);roll.setShowOriginalPitchLine(false);roll.setShowLyrics(false);roll.setShowNoteRange(true);roll.setShowNativeWaveforms(false);roll.setShowWaveforms(false);roll.diagnosticRefresh();
            auto left=.3+visual.from-visual.scoreFrom,p=.3+visual.p-visual.scoreFrom;
            auto yellow=roll.diagnosticNativeYellowBounds(note.id);
            // pixelForSeconds rounds to an integer (12 ms at 1400 px/s
            // ends at x=494.8). Compare float geometry to the float paint seam.
            check("yellow_bounds_are_support_to_clockP_not_nominal_C",std::abs(yellow.getX()-roll.diagnosticEdgeX(left))<.02&&std::abs(yellow.getRight()-roll.diagnosticEdgeX(p))<.02);
            // A 0.794 ms real lead is only 1.11 pixels here: rounding its
            // midpoint can select a pixel whose centre is already AFTER P,
            // while the rounded top-left corner contains no full-colour ink.
            // Keep the normal-zoom exact geometry assertion above, then resolve
            // narrow leads at the editor's supported zoom for strict RGB tests.
            // Only the isolated viewport translation/zoom changes, never P/clock.
            if(yellow.getWidth()<8.f)
            {
                const auto zoom=std::min(8000.0,8.0/(visual.p-visual.from));
                const auto placed=420.0/zoom;
                view.tracks[0].clips[0].startSeconds=placed-note.startSeconds;
                model.resetDocument(view);roll.setPixelsPerSecond(float(zoom));roll.diagnosticRefresh();
                left=placed+visual.from-visual.scoreFrom;p=placed+visual.p-visual.scoreFrom;
                yellow=roll.diagnosticNativeYellowBounds(note.id);
                check("narrow_P_resolved_zoom_keeps_exact_support_and_P",std::abs(yellow.getX()-roll.diagnosticEdgeX(left))<.02&&std::abs(yellow.getRight()-roll.diagnosticEdgeX(p))<.02);
                log+="resolved_pixel_zoom="+juce::String(zoom,2)+" prefix_px="+juce::String(yellow.getWidth(),4)+"\n";
            }
            const int x=int(std::floor((yellow.getX()+yellow.getRight())*.5f)),y=int(roll.diagnosticYForMidi(note.midiNote))-6;
            for(const auto style:{PianoRollComponent::NoteStyle::mld,PianoRollComponent::NoteStyle::rep})
            {
                roll.setNoteStyle(style);juce::Image image(juce::Image::ARGB,1100,80,true,juce::SoftwareImageType{});juce::Graphics g(image);g.addTransform(juce::AffineTransform::translation(0.f,float(40-y)));roll.paintEntireComponent(g,true);
                const auto ink=image.getPixelAt(x,40);
                check("real_existing_P_prefix_is_yellow_pixel",ink.getRed()>ink.getBlue()+20&&ink.getGreen()>ink.getBlue()+10);
                const auto edge=image.getPixelAt(x,int(std::floor(yellow.getY()+.5f))+40-y);
                check("yellow_prefix_outline_not_purple",edge.getRed()>edge.getBlue()+15&&edge.getGreen()>edge.getBlue()+8);
                juce::PNGImageFormat png;if(auto stream=output.getChildFile("yellow-"+juce::String(positiveP)+(style==PianoRollComponent::NoteStyle::rep?"-rep.png":"-mld.png")).createOutputStream())png.writeImageToStream(image,*stream);
            }
            // Segment role is not pronunciation identity: neither a legacy
            // transition role nor simplified-consonant may suppress a real P.
            for(const bool simplified:{false,true})
            {
                auto ordinary=view;auto& on=ordinary.tracks[0].clips[0].notes[0];on.nativeRole=NativeSegmentRole::transition;on.simplifiedConsonant=simplified;
                model.resetDocument(ordinary);roll.diagnosticRefresh();
                check("nativeRole_and_simplified_flag_do_not_hide_existing_P",std::abs(roll.diagnosticNativeYellowBounds(note.id).getWidth()-yellow.getWidth())<.02);
            }
            // Whole-pronunciation type wins over carried P, nativeRole and the
            // simplified-consonant preference. No name-based transition guess.
            for(const bool simplified:{false,true})
            {
                auto typed=view;auto& tn=typed.tracks[0].clips[0].notes[0];tn.pronunciationKind=PronunciationKind::transition;tn.simplifiedConsonant=simplified;tn.nativeRole=NativeSegmentRole::consonant;
                model.resetDocument(typed);roll.diagnosticRefresh();
                check("typed_transition_has_no_yellow_despite_carried_P",roll.diagnosticNativeYellowBounds(note.id).isEmpty());
                juce::Image image(juce::Image::ARGB,1100,80,true,juce::SoftwareImageType{});juce::Graphics g(image);g.addTransform(juce::AffineTransform::translation(0.f,float(40-y)));roll.paintEntireComponent(g,true);
                const auto ink=image.getPixelAt(x,40);check("typed_transition_entire_prefix_green",ink.getGreen()>ink.getRed()+10&&ink.getGreen()>ink.getBlue());
            }
        }
    }
    check("real_project_contains_positive_P_evidence",positiveP>0);
    check("P_matches_explicit_graph_anchor_not_voiced_onset_or_attack",pEvidence);
    check("source_P_override_contains_source_seconds_not_target_seconds",sourceUnits);
    check("native_C_equals_warp_projected_P",consonantP);
    check("ordinary_voiced_from_start_can_have_yellow_lead",ordinaryVoicedLeads>0);
    check("explicit_positive_and_zero_anchors_are_distinct",positiveAnchors>0&&zeroAnchors>0);
    check("view_keeps_source_clock_exact",clocks);check("UV_mask_unchanged_no_synthesized_voicing",uv);check("input_project_read_only",juce::SHA256(mpd).toHexString()==sha);
    // Independent missing-metadata regression: later stable F0 MUST NOT invent
    // a synchronization anchor. Also verify explicit zero and a carried source
    // P while F0 is voiced at time zero, without mutating the imported project.
    if(!raw.tracks.empty()&&!raw.tracks[0].clips.empty())
    {
        const auto seed=raw.tracks[0].clips[0];
        for(const int kind:{0,1,2})
        {
            ProjectData fixture;auto tr=raw.tracks[0];tr.clips={seed};fixture.tracks={tr};
            auto& fn=fixture.tracks[0].clips[0].notes[0];
            fn.nativeSegments.clear();fn.nativeSourceStartSeconds=-1;fn.nativeSourceEndSeconds=-1;
            fn.utauPreutteranceOverrideEnabled=kind!=0;
            fn.utauPreutteranceSeconds=kind==2?seed.sourceDurationSeconds*.2:0.0;
            for(auto& p:fn.contour)p.voiced=kind==2||p.timeSeconds>fn.durationSeconds*.5;
            const auto sourceP=seed.sourceOffsetSeconds+fn.utauPreutteranceSeconds;
            const auto before=nativeClipClock(fixture.tracks[0].clips[0]);
            juce::StringArray ignored;SampleSettings::convertMelodyneProject(fixture,ignored,false,false);
            const auto& fc=fixture.tracks[0].clips[0];const auto& nn=fc.notes[0];const auto span=nativeNoteVisualSpan(nn,fc);
            const auto wanted=fc.startSeconds+nativeTargetTimeAt(before,sourceP-fc.sourceOffsetSeconds);
            check(kind==0?"missing_anchor_late_F0_does_not_fabricate_P":kind==1?"explicit_zero_anchor_does_not_fabricate_yellow":"explicit_source_P_works_without_unvoiced_prefix",std::abs(span.p-wanted)<1.e-7);
        }
    }
    log+="notes="+juce::String(notes)+" positiveP="+juce::String(positiveP)+" positiveAnchors="+juce::String(positiveAnchors)+" zeroAnchors="+juce::String(zeroAnchors)+" missingAnchors="+juce::String(missingAnchors)+" ordinaryVoicedLeads="+juce::String(ordinaryVoicedLeads)+" stableLater="+juce::String(stableLater)+" voicedFromStart="+juce::String(voicedFromStart)+" noStable="+juce::String(noStable)+"\n";
    std::cout<<log<<std::endl;return output.getChildFile("verification.txt").replaceWithText(log)&&ok;
}
}
