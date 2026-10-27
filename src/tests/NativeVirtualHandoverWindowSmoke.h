#pragma once
#include "../NativeVirtualGroupIO.h"
#include "../NativeVirtualChainExtension.h"
#include "../AudioEngine.h"
#include <iostream>
#include <limits>
namespace hachi
{
// Hook in Main as --smoke-native-virtual-handover-window <OWN-output>.
// No NN, rendering, real source mutation or GUI dependency.
inline bool runNativeVirtualHandoverWindowSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto near=[](double a,double b){return std::abs(a-b)<1.e-9;};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.compose=true;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");
        clip.startSeconds=i*.8;clip.durationSeconds=clip.sourceDurationSeconds=1.;clip.sourceTimeMap={{0,0},{1,1}};
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=1.;clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);
    const auto id=model.createNativeVirtualConnection({"note0","note1"},false,error);
    check("create_overlapping_route",id.isNotEmpty());if(id.isEmpty())return false;
    auto state=model.snapshot();const auto group=state.nativeVirtualGroups[0];NativeVirtualGroup draft;NativeVirtualConnectionPlan geometry;
    const auto before=model.contentFingerprint();const auto revision=model.revisionNumber();
    check("pure_preview_reuses_canonical_auto_boundaries",prepareNativeVirtualHandoverWindow(track,group,"note0","note1",.85,.95,draft,error)
        &&model.contentFingerprint()==before&&resolveNativeVirtualGroup(track,draft,geometry,error)&&near(geometry.handovers[0].from,.85)&&near(geometry.handovers[0].to,.95));
    check("one_atomic_model_commit",model.setNativeVirtualHandoverWindow(id,"note0","note1",.85,.95,error)&&model.revisionNumber()==revision+1);
    const auto edited=model.contentFingerprint();model.undo();check("one_undo_exact",model.contentFingerprint()==before);model.redo();check("redo_exact",model.contentFingerprint()==edited);
    check("same_commit_is_noop",!model.setNativeVirtualHandoverWindow(id,"note0","note1",.85,.95,error)&&error.isEmpty()&&model.contentFingerprint()==edited);
    check("nonfinite_rejected_atomically",!model.setNativeVirtualHandoverWindow(id,"note0","note1",std::numeric_limits<double>::quiet_NaN(),.95,error)&&error.isNotEmpty()&&model.contentFingerprint()==edited);
    check("expanded_overlap_rejected_atomically",!model.setNativeVirtualHandoverWindow(id,"note0","note1",.79,.95,error)&&model.contentFingerprint()==edited);
    check("reverse_or_unknown_identity_rejected",!model.setNativeVirtualHandoverWindow(id,"note1","note0",.85,.95,error)&&!model.setNativeVirtualHandoverWindow(id,"note0","note2",.85,.95,error)&&model.contentFingerprint()==edited);
    check("collapsed_window_rejected",!model.setNativeVirtualHandoverWindow(id,"note0","note1",.9,.9,error)&&model.contentFingerprint()==edited);
    auto encoded=nativeVirtualGroupTree(model.snapshot().nativeVirtualGroups[0]);NativeVirtualGroup decoded;
    check("strict_io_roundtrip",nativeVirtualGroupFromTree(encoded,decoded,error)&&decoded.handoverWindows.size()==1&&near(decoded.handoverWindows[0].fromOffsetSeconds,.05)&&near(decoded.handoverWindows[0].toOffsetSeconds,-.05));
    const auto window=encoded.getChildWithName("VirtualHandoverWindow");encoded.addChild(window.createCopy(),-1,nullptr);
    check("duplicate_record_rejected",!nativeVirtualGroupFromTree(encoded,decoded,error));
    encoded=nativeVirtualGroupTree(model.snapshot().nativeVirtualGroups[0]);encoded.getChildWithName("VirtualHandoverWindow").setProperty("fromOffsetSeconds","0.05",nullptr);
    check("numeric_string_rejected",!nativeVirtualGroupFromTree(encoded,decoded,error));
    auto invalid=group;invalid.handoverWindows={{"note0","note2",.01,-.01}};
    check("orphan_record_rejected",!nativeVirtualGroupFromTree(nativeVirtualGroupTree(invalid),decoded,error));
    check("legacy_missing_window_unchanged",nativeVirtualGroupFromTree(nativeVirtualGroupTree(group),decoded,error)&&decoded.handoverWindows.empty());
    auto hashGroups=model.snapshot().nativeVirtualGroups;
    const auto baseHash=AudioEngine::nativeClipWaveformHash(track.clips[0],track);
    const auto changedHash=AudioEngine::nativeClipWaveformHash(track.clips[0],track,&hashGroups);
    hashGroups[0].enabled=false;
    check("enabled_windows_change_hash_but_disabled_groups_leave_original_hash",changedHash!=baseHash&&AudioEngine::nativeClipWaveformHash(track.clips[0],track,&hashGroups)==baseHash);
    hashGroups[0].enabled=true;hashGroups[0].handoverWindows[0].fromOffsetSeconds+=.01;
    check("window_field_participates_in_enabled_hash",AudioEngine::nativeClipWaveformHash(track.clips[0],track,&hashGroups)!=changedHash);
    auto movedTrack=track;movedTrack.clips[1].startSeconds=.85;
    check("moved_window_projects_strictly_inside_current_support",resolveNativeVirtualGroup(movedTrack,model.snapshot().nativeVirtualGroups[0],geometry,error)&&near(geometry.handovers[0].from,.9)&&near(geometry.handovers[0].to,.95));
    movedTrack.clips[1].startSeconds=2.;const auto stored=model.snapshot().nativeVirtualGroups[0];
    bool audible=false;const bool resolved=resolveNativeVirtualGroup(movedTrack,stored,geometry,error);
    if(resolved)audible=nativeVirtualMemberContribution(geometry,0,.5)>0&&nativeVirtualMemberContribution(geometry,1,2.5)>0;
    check("unsupported_override_retains_identity_warns_and_does_not_mute_route",resolved&&geometry.geometryNotice.contains("not applied")&&audible&&stored.handoverWindows.size()==1);
    check("gap_cannot_create_window",!prepareNativeVirtualHandoverWindow(movedTrack,group,"note0","note1",1.,2.,draft,error));
    movedTrack.clips[1].startSeconds=1.;check("touch_cannot_create_window",!prepareNativeVirtualHandoverWindow(movedTrack,group,"note0","note1",1.,1.01,draft,error));
    check("extension_keeps_window_endpoint_ids",model.setNativeVirtualConnectionForNotes({"note1","note2"},true,false,error)
        &&model.snapshot().nativeVirtualGroups[0].handoverWindows.size()==1&&model.snapshot().nativeVirtualGroups[0].handoverWindows[0].leftNoteId=="note0");
    const auto saved=folder.getChildFile("handover.hjpx");ProjectModel cold;
    check("cold_model_roundtrip",model.save(saved,error)&&cold.load(saved,error)&&cold.snapshot().nativeVirtualGroups[0].handoverWindows.size()==1);
    const auto beforeReset=model.contentFingerprint();check("reset_exact_window",model.resetNativeVirtualHandoverWindow(id,"note0","note1",error)&&model.snapshot().nativeVirtualGroups[0].handoverWindows.empty());
    model.undo();check("reset_is_single_undo",model.contentFingerprint()==beforeReset);
    // Use planner-only typed geometry to exercise retained fallback: even when
    // the old planner uses the real overlap, a manual window cannot cross middle.
    NativeVirtualConnectionPlan typed;typed.retainedTopology=true;
    NativeVirtualMember a,b;a.noteId="left";a.from=0;a.to=2;b.noteId="bridge";b.from=1;b.to=3;b.middle=1.5;b.transition=true;
    typed.members={a,b};typed.handovers={{"left","bridge",1,2,false}};
    NativeVirtualGroup typedGroup;typedGroup.orderedNoteIds={"left","bridge"};typedGroup.handoverWindows={{"left","bridge",.1,-.1}};
    check("typed_middle_is_strict_projection_limit",applyNativeVirtualHandoverWindows(typedGroup,typed,error)&&near(typed.handovers[0].from,1.1)&&near(typed.handovers[0].to,1.5));
    typed.handovers={{"left","bridge",1.8,2,false}};
    check("typed_side_without_support_keeps_automatic_geometry_and_warns",applyNativeVirtualHandoverWindows(typedGroup,typed,error)&&typed.geometryNotice.contains("not applied")&&near(typed.handovers[0].from,1.8));
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
