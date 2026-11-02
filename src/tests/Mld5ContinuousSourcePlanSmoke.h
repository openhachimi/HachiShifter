#pragma once
#include "../Mld5ContinuousSource.h"
#include <iostream>

namespace hachi
{
// Handwritten facts only. Imaginary paths deliberately prove planning has no
// dependency on file existence, media decoding, models or an audio device.
inline bool runMld5ContinuousSourcePlanSmoke(const juce::File& out)
{
    using Request = backend::Mld5FileRenderRequest;
    struct Fixture
    {
        ClipData a, b; Request x, y;
        std::map<juce::String, juce::String> owners;
        Mld5ContinuousSourcePlan plan() const
        { return planMld5ContinuousSource({ &a, &b }, { x, y }, owners); }
    };
    const auto fixture = [](bool overlap)
    {
        Fixture f;
        f.a.id = "left"; f.b.id = "right";
        f.a.sourceFile = f.b.sourceFile = juce::File::getCurrentWorkingDirectory().getChildFile("imaginary-mld5-source.wav");
        f.a.startSeconds = 3; f.b.startSeconds = 3.5;
        f.a.durationSeconds = f.a.sourceDurationSeconds = overlap ? .52 : .5;
        f.b.durationSeconds = f.b.sourceDurationSeconds = .5; f.b.sourceOffsetSeconds = .5;
        f.a.materialSourceUsageGroupId = f.b.materialSourceUsageGroupId = "one-source-use";
        f.a.materialSourceFingerprint = f.b.materialSourceFingerprint = "handwritten-source-byte-identity";
        NoteData n; n.materialAudioId = "annotation-variant-A"; f.a.notes = f.b.notes = { n };
        // Nonlinear only before the common overlap: target F0 must NOT be
        // source-warped a second time when the request curves are concatenated.
        f.a.sourceTimeMap = { {0,0}, {.25,.2}, {.5,.5} };
        if (overlap) f.a.sourceTimeMap.push_back({.52,.52});
        f.b.sourceTimeMap = { {0,0}, {.5,.5} };
        const auto request = [](const ClipData& c)
        {
            Request r; r.sourceFile = c.sourceFile; r.sourceOffsetSeconds = c.sourceOffsetSeconds;
            r.sourceDurationSeconds = c.sourceDurationSeconds; r.targetDurationSeconds = c.durationSeconds;
            r.pitchBackend = backend::PitchRenderBackend::mld5; r.preserveUneditedSource = true;
            for (const auto& p : c.sourceTimeMap) r.timeMap.push_back({p.targetSeconds,p.sourceSeconds});
            const auto count = std::size_t(std::ceil(c.durationSeconds / .005)) + 1;
            for (std::size_t k = 0; k < count; ++k)
            {
                const double t = c.startSeconds - 3 + std::min(c.durationSeconds,double(k)*.005);
                r.sourceMidi.push_back(float(60+2*t)); r.targetMidi.push_back(float(64+3*t));
                r.formantSemitones.push_back(.75f); r.noteGain.push_back(.8f);
                r.tension.push_back(.1f); r.breath.push_back(.2f); r.robustPitchCurve.push_back(0);
            }
            return r;
        };
        f.x = request(f.a); f.y = request(f.b);
        f.owners = { {f.a.id,"physical-A"}, {f.b.id,"physical-B"} }; return f;
    };
    bool ok = true; juce::String log;
    const auto check = [&](const char* name, bool pass)
    { ok &= pass; log << name << "=" << (pass ? "PASS" : "FAIL") << "\n"; std::cout << name << "=" << pass << std::endl; };
    const auto rejected = [&](const char* name, const Fixture& f)
    { const auto p = f.plan(); check(name,!p.ready && p.error.isNotEmpty() && p.clips.empty()); };
    const auto near = [](double a,double b) { return std::abs(a-b) < 1.e-5; };
    auto adjacent = fixture(false); const auto a = adjacent.plan();
    check("coherent_source_usage_adjacent_without_glide",a.ready && near(a.merged.targetDurationSeconds,1));
    auto overlap = fixture(true); const auto p = overlap.plan();
    check("20ms_overlap_is_one_second_union_not_1_02",p.ready && near(p.startSeconds,3) && near(p.endSeconds,4)
        && near(p.merged.targetDurationSeconds,1) && near(p.merged.sourceDurationSeconds,1));
    bool forward = p.ready;
    for (std::size_t i=1;i<p.merged.timeMap.size();++i)
        forward &= p.merged.timeMap[i].targetSeconds > p.merged.timeMap[i-1].targetSeconds
            && p.merged.timeMap[i].sourceSeconds > p.merged.timeMap[i-1].sourceSeconds;
    check("merged_map_is_strictly_forward_and_has_original_middle_anchor",forward && p.merged.timeMap.size() >= 4
        && near(mld5_continuous_detail::sourceAt(p.merged,.25),.2));
    check("source_and_target_F0_stay_on_target_grid_no_extra_warp",p.ready && p.merged.sourceMidi.size()==201
        && near(p.merged.sourceMidi[50],60.5) && near(p.merged.targetMidi[50],64.75)
        && near(p.merged.sourceMidi[102],61.02) && near(p.merged.targetMidi[102],65.53));
    check("authored_controls_and_metadata_inherited_without_forced_glide",p.ready && p.merged.isGlideMerged
        && p.merged.preserveUneditedSource && near(p.merged.noteGain[102],.8) && near(p.merged.formantSemitones[102],.75));
    check("planner_does_not_mutate_inputs",overlap.a.sourceTimeMap.size()==4 && overlap.x.timeMap.size()==4
        && near(overlap.x.targetDurationSeconds,.52) && overlap.x.sourceMidi.size()==105 && !overlap.x.isGlideMerged);
    auto f = fixture(true); f.a.materialSourceUsageGroupId.clear(); f.b.materialSourceUsageGroupId.clear();
    f.a.nativePitchGroupId = f.b.nativePitchGroupId = "not-a-source-identity";
    rejected("independent_same_WAV_and_pitch_group_different_owner_rejected",f);
    f.owners[f.b.id] = f.owners[f.a.id]; check("same_physical_owner_without_glide_or_usage_accepted",f.plan().ready);
    f = fixture(true); f.a.materialSourceUsageGroupId.clear(); f.b.materialSourceUsageGroupId.clear();
    f.a.glideConnectedToNext = f.b.glideConnectedFromPrevious = true;
    check("explicit_bidirectional_glide_with_strict_clock_evidence",f.plan().ready);
    f.b.glideConnectedFromPrevious = false; rejected("one_way_glide_is_not_source_identity",f);
    f=fixture(true);f.b=f.a;f.b.id="pasted-layer";f.y=f.x;f.owners[f.b.id]="independent-pasted-owner";rejected("exact_duplicate_layer_is_not_merged_even_if_usage_provenance_was_copied",f);
    f = fixture(true); f.b.sourceFile = f.y.sourceFile = f.b.sourceFile.getSiblingFile("different-physical.wav");
    rejected("different_physical_file_even_same_fingerprint_rejected",f);
    f = fixture(true); f.b.materialSourceFingerprint = "different-bytes"; rejected("different_source_fingerprint_rejected",f);
    f = fixture(true); f.b.notes[0].materialAudioId = "annotation-variant-B"; rejected("different_annotation_variants_rejected",f);
    f = fixture(true); f.a.sourceTimeMap.insert(f.a.sourceTimeMap.end()-1,{.51,.512});
    f.x.timeMap.insert(f.x.timeMap.end()-1,{.51,.512});
    rejected("conflicting_middle_source_anchor_despite_matching_overlap_endpoints",f);
    f = fixture(true); f.y.targetMidi[2] += 1; rejected("conflicting_middle_overlap_target_F0_rejected",f);
    f = fixture(true); f.y.sourceMidi[2] += 1; rejected("conflicting_overlap_source_F0_rejected",f);
    f = fixture(true); f.y.noteGain[2] = .4f; rejected("different_authored_overlap_gain_rejected",f);
    f = fixture(true); f.y.formantSemitones[2] += 1; rejected("different_overlap_formant_rejected",f);
    f = fixture(true); f.y.tension[2] += .1f; rejected("different_overlap_tension_rejected",f);
    f = fixture(true); f.y.breath[2] += .1f; rejected("different_overlap_breath_rejected",f);
    f = fixture(true); f.y.robustPitchCurve[2] = 1; rejected("different_overlap_robust_region_rejected",f);
    f = fixture(false); f.b.startSeconds += .001; rejected("target_gap_rejected",f);
    f = fixture(false); f.b.sourceOffsetSeconds = f.y.sourceOffsetSeconds = .501;
    rejected("touch_requires_same_source_samplepoint",f);
    f = fixture(true); f.y.framePeriodMs = 10; rejected("different_frame_period_rejected",f);
    f = fixture(true); f.y.normalizeVolume = true; rejected("different_engine_options_rejected",f);
    f = fixture(true); f.y.pitchBackend = backend::PitchRenderBackend::world; rejected("different_engine_rejected",f);
    f = fixture(true); f.y.targetDurationSeconds += .001; rejected("clip_request_duration_mismatch_rejected",f);
    f = fixture(true); f.a.sourceTimeMap.insert(f.a.sourceTimeMap.begin()+1,{0,.001});
    rejected("duplicate_target_conflict_not_sort_max_repaired",f);
    f = fixture(true); f.x.sourceOffsetSeconds = f.a.sourceOffsetSeconds = .6;
    rejected("positive_but_backwards_source_offset_rejected",f);
    f = fixture(true); f.y.sourceOffsetSeconds = f.b.sourceOffsetSeconds = -.01; rejected("negative_source_offset_rejected",f);
    f = fixture(true); f.y.preserveUneditedSource = false;
    check("preserve_unedited_source_requires_all_members",f.plan().ready && !f.plan().merged.preserveUneditedSource);
    f = fixture(true); f.x.sourceMidi.clear(); f.y.sourceMidi.clear(); f.x.targetMidi.clear(); f.y.targetMidi.clear();
    const auto uv = f.plan(); check("empty_UV_curves_remain_zero",uv.ready
        && std::all_of(uv.merged.sourceMidi.begin(),uv.merged.sourceMidi.end(),[](float v){return v==0;})
        && std::all_of(uv.merged.targetMidi.begin(),uv.merged.targetMidi.end(),[](float v){return v==0;}));
    check("single_clip_not_ready_with_error",!planMld5ContinuousSource({&f.a},{f.x},f.owners).ready);
    if (out.createDirectory().failed()) return false;
    return out.getChildFile("verification.txt").replaceWithText(log) && ok;
}
}
