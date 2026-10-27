#pragma once
#include "../MainComponent.h"
#include "../OttoMaterialIO.h"
#include "../NativeSourceEdgePlan.h"
#include "../NativeNoteTiming.h"
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
// Included once by MainComponent.cpp: actual AssetManager producer and Main
// drop/load consumers, not a fabricated drag payload or pure-planner-only test.
bool MainComponent::diagnosticNativeMaterialOverlapUi(const juce::File& output)
{
    const auto root=output.getChildFile("native-material-overlap-"+juce::Uuid().toString());
    if(root.createDirectory().failed())return false;
    stopTimer();audio.stop();bool ok=true;juce::String log,error;
    const auto check=[&](const juce::String& name,bool pass){ok&=pass;log+=name+"="+(pass?"PASS":"FAIL")+"\n";std::cout<<name<<'='<<pass<<std::endl;};
    const auto finish=[&]{const bool written=root.getChildFile("verification.txt").replaceWithText(log);std::cout<<"native_material_overlap_fixture="<<root.getFullPathName()<<std::endl;return ok&&written;};
#if !JUCE_WINDOWS
    check("actual_windows_host_message_pump",false);return finish();
#else
    const auto pump=[&]
    {
        MSG message{};for(int i=0;i<128&&PeekMessageW(&message,nullptr,0,0,PM_REMOVE);++i)
        {if(message.message==WM_QUIT){PostQuitMessage(static_cast<int>(message.wParam));return false;}TranslateMessage(&message);DispatchMessageW(&message);}
        project.dispatchPendingMessages();return true;
    };
    const auto wait=[&](const std::function<bool()>& ready,int milliseconds)
    {const auto end=juce::Time::getMillisecondCounterHiRes()+milliseconds;while(!ready()&&juce::Time::getMillisecondCounterHiRes()<end){if(!pump())return false;juce::Thread::sleep(2);}pump();return ready();};
    const auto closeWarnings=[&]{for(int i=0;i<8;++i){auto* modal=juce::Component::getCurrentlyModalComponent();if(!modal)break;modal->exitModalState(0);pump();}};
    const auto eq=[](double a,double b){return std::abs(a-b)<1.e-7;};
    const auto smooth=[](double x){x=juce::jlimit(0.0,1.0,x);return float(x*x*(3-2*x));};
    const auto source=root.getChildFile("a_b_c.wav"),manifest=root.getChildFile("otto.ini");
    juce::AudioBuffer<float> pcm(1,120000);
    for(int i=0;i<pcm.getNumSamples();++i)pcm.setSample(0,i,.12f*std::sin(juce::MathConstants<double>::twoPi*173*i/48000.0));
    auto stream=source.createOutputStream();juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.release(),48000,1,24,{},0):nullptr);
    check("physical_sine_PCM_control_not_ASR_oracle",writer&&writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()));writer.reset();if(!ok)return finish();
    std::vector<SampleRegionSetting> rows;
    for(int i=0;i<3;++i)
    {
        SampleRegionSetting row;row.materialAudioId="one-exact-variant";row.materialRegionId=juce::String::charToString(char('a'+i));row.name=row.materialRegionId;
        row.regionStartSeconds=.2+.5*i;row.regionEndSeconds=.9+.5*i;row.alignmentSeconds=.3+.5*i;row.fixedDurationSeconds=.2;row.overlapSeconds=.12;
        row.nativeUnpitched=true;row.role=NativeSegmentRole::vowel;row.provenance="explicit-test-support";
        NativePronunciationEdges edge;edge.entryFromSourceSeconds=row.regionStartSeconds;edge.entryToSourceSeconds=row.regionStartSeconds+.14;
        edge.exitFromSourceSeconds=row.regionEndSeconds-.18;edge.exitToSourceSeconds=edge.terminationSourceSeconds=row.regionEndSeconds;edge.provenance="explicit-test-anchors";row.nativePronunciationEdges=edge;
        rows.push_back(row);
    }
    OttoMaterialIO::Document document;document.audio={{rows.front().materialAudioId,source.getFileName(),rows}};
    check("legitimate_overlapping_support_manifest",OttoMaterialIO::saveDocument(manifest,document,error));if(!ok)return finish();
    const auto sourceHash=juce::SHA256(source).toHexString(),manifestHash=juce::SHA256(manifest).toHexString();
    setAssetManagerVisible(true);check("real_asset_manager_folder_registered",assetManager&&assetManager->diagnosticRegisterFolder(root)>=0);if(!assetManager)return finish();
    check("actual_load_selection_consumer_bound",bool(assetManager->onLoadMaterialSelection));
    TrackData track;track.id="overlap-host";track.trackType=TrackType::hachi;track.compose=true;track.nativeNsfAudio=true;
    track.pitchAlgorithm=PitchAlgorithm::nsfHifigan;track.normalizeVolume=false;track.smoothOverlaps=true;
    ProjectData seed;seed.tracks={track};
    const auto render=[&](TrackData current,juce::AudioBuffer<float>& result)
    {
        // The exact placement is retained; merely start this offline capture at
        // its own clip origin. The control has ONE physical union owner.
        current.clips[0].startSeconds=0;ProjectData data;data.tracks={std::move(current)};
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);
        if(!wait([&]{const auto p=engine.renderProgress();return !p||*p>=.999;},30000)||engine.activeRenderWarnings().isNotEmpty())return false;
        const int samples=juce::roundToInt(data.tracks[0].clips[0].durationSeconds*48000);result.setSize(2,samples);result.clear();engine.setPosition(0);engine.play();
        for(int offset=0;offset<samples;offset+=257)engine.getNextAudioBlock({&result,offset,std::min(257,samples-offset)});
        engine.stop();return engine.activeRenderWarnings().isEmpty();
    };
    struct Selection {const char* name;int first,count;bool drag;};
    for(const auto selection:{Selection{"abc",0,3,true},Selection{"ab",0,2,true},Selection{"bc",1,2,false},Selection{"a",0,1,false},Selection{"b",1,1,true}})
    {
        project.resetDocument(seed);project.dispatchPendingMessages();selectedTrackId=track.id;pianoRoll.setFocusedTrack(track.id);audio.setPosition(0);setSize(1320,850);resized();
        juce::StringArray ids;for(int i=0;i<selection.count;++i)ids.add(rows[std::size_t(selection.first+i)].materialRegionId);
        const auto revision=project.revisionNumber();
        if(selection.drag)
        {
            // Empty IDs is the real whole-recording manager producer path.
            const auto payload=assetManager->pronunciationDragDescription(source,selection.count==3?juce::StringArray{}:ids,rows.front().materialAudioId);
            const auto point=pianoViewport.getPosition()+juce::Point<int>(58,40);
            juce::DragAndDropTarget::SourceDetails drop(payload,assetManager.get(),point);
            check(juce::String(selection.name)+"_real_manager_drag_accepted",isInterestedInDragSource(drop));itemDropped(drop);
        }
        else if(assetManager->onLoadMaterialSelection)
            assetManager->onLoadMaterialSelection({source,root,rows.front().materialAudioId,ids});
        closeWarnings();const auto placed=project.snapshot();
        const auto prefix=juce::String(selection.name)+"_";
        const bool inserted=placed.tracks.size()==1&&placed.tracks[0].clips.size()==1&&placed.tracks[0].clips[0].notes.size()==std::size_t(selection.count);
        check(prefix+"actual_ProjectModel_placement_not_blanket_overlap_error",inserted&&project.revisionNumber()==revision+1);
        check(prefix+"production_analysis_completed",wait([&]{return pendingNativeAnalyses==0;},180000));
        if(!inserted)continue;
        const auto& clip=placed.tracks[0].clips[0];const auto& first=rows[std::size_t(selection.first)];const auto& last=rows[std::size_t(selection.first+selection.count-1)];
        check(prefix+"physical_union_heard_once_identity_clock",clip.parts.empty()&&eq(clip.sourceOffsetSeconds,first.regionStartSeconds)
            &&eq(clip.durationSeconds,last.regionEndSeconds-first.regionStartSeconds)&&eq(clip.sourceDurationSeconds,clip.durationSeconds)
            &&eq(nativeSourceTimeAt(nativeClipClock(clip),clip.durationSeconds*.5),clip.durationSeconds*.5));
        bool snapshots=true,bodies=true;
        for(std::size_t i=0;i<clip.notes.size();++i)
        {
            const auto& note=clip.notes[i];const auto& row=rows[std::size_t(selection.first)+i];
            snapshots&=note.materialAudioId==row.materialAudioId&&note.materialRegionId==row.materialRegionId
                &&eq(note.materialSourceStartSeconds,row.regionStartSeconds)&&eq(note.materialSourceEndSeconds,row.regionEndSeconds)
                &&note.nativePronunciationEdges==row.nativePronunciationEdges;
            const auto begin=i==0?row.regionStartSeconds:row.alignmentSeconds;
            const auto end=i+1==clip.notes.size()?row.regionEndSeconds:rows[std::size_t(selection.first)+i+1].alignmentSeconds;
            bodies&=eq(note.startSeconds,begin-first.regionStartSeconds)&&eq(note.durationSeconds,end-begin)
                &&eq(note.nativeSourceStartSeconds,begin)&&eq(note.nativeSourceEndSeconds,end);
            if(i+1<clip.notes.size())bodies&=note.materialConnectedNextRegionId==clip.notes[i+1].materialRegionId;
        }
        check(prefix+"authored_support_profiles_and_IDs_preserved",snapshots);check(prefix+"actual_owned_bodies_and_retained_relation",bodies);
        NativeSourceEdgePlan plan;const bool planned=resolveNativeSourceEdgePlan(placed.tracks[0],{},plan,error);
        bool mask=planned&&plan.components.size()==1&&plan.members.size()==std::size_t(selection.count);
        for(std::size_t i=0;mask&&i<plan.members.size();++i)mask&=plan.members[i].exposedEntry==(i==0)&&plan.members[i].exposedExit==(i+1==plan.members.size());
        check(prefix+"current_edge_plan_only_actual_outer_owners",mask);
        if(selection.count==3)
        {
            // Start from the ACTUAL overlapped manager placement, whose retained
            // body is intentionally smaller than each full annotation support.
            auto publicationData=placed;
            auto foreign=clip;foreign.id+="-foreign";foreign.startSeconds+=3;
            for(auto& note:foreign.notes){note.id+="-foreign";note.materialAudioId="foreign-variant";}
            auto bare=clip;bare.id+="-bare";bare.startSeconds+=6;
            for(auto& note:bare.notes)
            {note.id+="-bare";note.materialAudioId.clear();note.materialRegionId.clear();note.materialConnectedNextRegionId.clear();note.nativePronunciationEdges.reset();}
            publicationData.tracks[0].clips.push_back(std::move(foreign));publicationData.tracks[0].clips.push_back(std::move(bare));
            ProjectModel publication;publication.resetDocument(publicationData);
            const auto before=publication.snapshot();const auto beforeFingerprint=publication.contentFingerprint();const auto beforeRevision=publication.revisionNumber();
            auto renamed=rows;renamed[1].name="b-published-new-alias";
            const auto preflight=publication.applySourceSettingsWithReport(source,renamed,false,rows.front().materialAudioId);
            check("abc_overlap_scoped_alias_preflight_is_nonmutating",!preflight.rejected&&preflight.changed()
                &&publication.contentFingerprint()==beforeFingerprint&&publication.revisionNumber()==beforeRevision&&!publication.canUndo());
            const auto applied=publication.applySourceSettingsWithReport(source,renamed,true,rows.front().materialAudioId);
            const auto after=publication.snapshot();
            bool renamedOnly=!applied.rejected&&applied.changed()&&publication.revisionNumber()==beforeRevision+1
                &&after.tracks[0].clips.size()==before.tracks[0].clips.size();
            auto normalized=after;
            for(std::size_t ci=0;renamedOnly&&ci<before.tracks[0].clips.size();++ci)
                for(std::size_t ni=0;renamedOnly&&ni<before.tracks[0].clips[ci].notes.size();++ni)
                {
                    const auto& oldNote=before.tracks[0].clips[ci].notes[ni];const auto& newNote=after.tracks[0].clips[ci].notes[ni];
                    const bool target=ci==0&&oldNote.materialRegionId==rows[1].materialRegionId;
                    renamedOnly&=newNote.label==(target?renamed[1].name:oldNote.label)
                        &&newNote.materialBoundAlias==(target?renamed[1].name:oldNote.materialBoundAlias);
                    normalized.tracks[0].clips[ci].notes[ni].label=oldNote.label;
                    normalized.tracks[0].clips[ci].notes[ni].materialBoundAlias=oldNote.materialBoundAlias;
                }
            ProjectModel normalizedModel;normalizedModel.resetDocument(normalized);
            check("abc_overlap_scoped_alias_only_bound_ID_foreign_and_bare_unchanged",renamedOnly);
            // Full fingerprint after normalizing only the two allowed alias
            // fields covers retained bodies, full support snapshots, clocks,
            // pitch, automation, all fades and foreign/bare source owners.
            check("abc_overlap_alias_publication_preserves_all_nonalias_music_source_and_fades",renamedOnly
                &&normalizedModel.contentFingerprint()==beforeFingerprint);
            NativeSourceEdgePlan publishedPlan;
            check("abc_overlap_alias_publication_retains_actual_edge_component",resolveNativeSourceEdgePlan(after.tracks[0],{},publishedPlan,error)
                &&publishedPlan.components.size()==2&&publishedPlan.members.size()==6);
            check("abc_overlap_alias_publication_single_undo",publication.undo()&&!publication.canUndo()
                &&publication.contentFingerprint()==beforeFingerprint);
        }
        const auto saved=root.getChildFile(juce::String(selection.name)+".hjpx");ProjectModel cold;
        check(prefix+"HJPX_cold_roundtrip",project.save(saved,error)&&cold.load(saved,error));
        NativeSourceEdgePlan coldPlan;const auto coldData=cold.snapshot();
        check(prefix+"cold_current_edge_plan",!coldData.tracks.empty()&&resolveNativeSourceEdgePlan(coldData.tracks[0],{},coldPlan,error)
            &&coldPlan.components.size()==1&&coldPlan.members.size()==plan.members.size());
        check(prefix+"one_music_undo",project.undo()&&!project.canUndo()&&project.snapshot().tracks[0].clips.empty());
        // Physical-union PCM control: single bare source clip, no annotation row
        // overlaps, no internal fades. Normal engine, never a second renderer.
        auto actual=placed.tracks[0];auto control=actual;control.clips[0].notes.clear();
        juce::AudioBuffer<float> reference,heard;
        bool exact=render(control,reference)&&render(actual,heard)&&reference.getNumSamples()==heard.getNumSamples();
        float peak=0;for(int i=0;exact&&i<heard.getNumSamples();++i)
        {
            const double t=i/48000.0;const auto gain=smooth(t/.14)*smooth((clip.durationSeconds-t)/.18);
            for(int channel=0;channel<2;++channel){peak=std::max(peak,std::abs(reference.getSample(channel,i)));exact&=std::abs(heard.getSample(channel,i)-reference.getSample(channel,i)*gain)<2.e-6f;}
        }
        check(prefix+"normal_PCM_physical_union_times_outer_gain_no_double_internal",exact&&peak>.01f);
    }
    auto expressiveRows=rows;
    for(auto& row:expressiveRows)
    {
        row.segments={{"C-"+row.materialRegionId,row.name,NativeSegmentRole::consonant,row.regionStartSeconds,row.regionStartSeconds+.2,"user",1,row.alignmentSeconds,0,true,1},
            {"V-"+row.materialRegionId,row.name,NativeSegmentRole::vowel,row.regionStartSeconds+.2,row.regionEndSeconds,"user",1,row.regionStartSeconds+.2,0,true,1}};
        row.amplitudeEnvelope={{0,-6},{.1,-2},{.4,-3}};
    }
    ProjectModel expressive;expressive.resetDocument(seed);
    const auto expressiveId=expressive.addMaterialPronunciations(source,expressiveRows,{"a","b","c"},0,track.id,error);
    bool anchors=expressiveId.isNotEmpty();
    if(anchors)
    {
        const auto middle=expressive.snapshot().tracks[0].clips[0].notes[1];
        anchors=middle.nativeSegments.size()==2&&eq(middle.nativeSourceStartSeconds,.8)
            &&eq(middle.nativeSourceStartSeconds+middle.nativeSegments[0].sourceEndSeconds,.9)
            &&eq(middle.nativeSourceStartSeconds+middle.nativeSegments[1].sourceStartSeconds,.9)
            &&middle.amplitudeEnvelope.size()==3&&eq(middle.amplitudeEnvelope[0].timeSeconds,-.1)
            &&eq(middle.amplitudeEnvelope[1].timeSeconds,0)&&eq(middle.amplitudeEnvelope[2].timeSeconds,.3);
    }
    check("retained_CV_and_all_automation_marks_keep_physical_source_coordinates",anchors);
    // Model-level atomic negative cases complement (not replace) actual GUI
    // placement above. No codec relaxation or filesystem rewrite is required.
    ProjectModel invalid;invalid.resetDocument(seed);const auto fingerprint=invalid.contentFingerprint();const auto revision=invalid.revisionNumber();
    const auto rejects=[&](const juce::String& name,const std::vector<SampleRegionSetting>& candidate,const std::vector<juce::String>& ids)
    {check(name,invalid.addMaterialPronunciations(source,candidate,ids,0,track.id,error).isEmpty()&&error.isNotEmpty()&&invalid.contentFingerprint()==fingerprint&&invalid.revisionNumber()==revision&&!invalid.canUndo());};
    rejects("duplicate_IDs_atomic",rows,{"a","a"});rejects("reversed_overlap_atomic",rows,{"b","a"});
    auto nested=rows;nested[1].regionEndSeconds=.85;nested[1].nativePronunciationEdges.reset();nested[1].fixedDurationSeconds=.05;rejects("nested_ambiguous_overlap_atomic",nested,{"a","b"});
    auto variant=rows;variant[1].materialAudioId="unrelated-variant";rejects("cross_variant_overlap_atomic",variant,{"a","b"});
    auto noProof=rows;for(auto& row:noProof){row.nativePronunciationEdges.reset();row.overlapSeconds=0;}rejects("unsupported_overlap_atomic",noProof,{"a","b"});
    auto raw=rows;for(auto& row:raw)row.nativePronunciationEdges.reset();
    ProjectModel rawModel;rawModel.resetDocument(seed);
    check("explicit_raw_overlap_project_only_profiles",rawModel.addMaterialPronunciations(source,raw,{"a","b","c"},0,track.id,error).isNotEmpty());
    const auto rawData=rawModel.snapshot();NativeSourceEdgePlan rawPlan;
    check("raw_overlap_actual_retained_edge_plan",!rawData.tracks[0].clips.empty()&&resolveNativeSourceEdgePlan(rawData.tracks[0],{},rawPlan,error)&&rawPlan.components.size()==1);
    auto legacy=rows;for(std::size_t i=0;i<legacy.size();++i){auto& row=legacy[i];row.nativePronunciationEdges.reset();row.regionStartSeconds=.2+.5*i;row.regionEndSeconds=.7+.5*i;row.alignmentSeconds=row.regionStartSeconds+.1;row.overlapSeconds=4;}
    ProjectModel old;old.resetDocument(seed);const bool oldPlaced=old.addMaterialPronunciations(source,legacy,{"c","a","b"},0,track.id,error).isNotEmpty();
    bool absent=oldPlaced;for(const auto& c:old.snapshot().tracks[0].clips)for(const auto& n:c.notes)absent&=!n.nativePronunciationEdges;
    check("raw_overlap_marker_without_overlapping_body_keeps_legacy_NONE",oldPlaced&&absent);
    // Different physical files cannot be collapsed by usage proof even when
    // they share audio/region labels and exact source/target seam coordinates.
    const auto other=root.getChildFile("different-physical.wav");check("different_physical_fixture",source.copyFileTo(other));
    if(!rawData.tracks[0].clips.empty())
    {
        auto split=rawData.tracks[0];const auto original=split.clips[0];split.clips.clear();
        for(std::size_t i=0;i<original.notes.size();++i)
        {auto owner=original;owner.id="physical-owner-"+juce::String(int(i));owner.startSeconds=original.notes[i].startSeconds;owner.sourceOffsetSeconds=original.sourceOffsetSeconds+owner.startSeconds;owner.durationSeconds=owner.sourceDurationSeconds=original.notes[i].durationSeconds;owner.sourceTimeMap={{0,0},{owner.durationSeconds,owner.durationSeconds}};owner.notes={original.notes[i]};owner.notes[0].startSeconds=0;if(i==1)owner.sourceFile=other;split.clips.push_back(std::move(owner));}
        NativeSourceEdgePlan physical;check("different_physical_sources_never_merged",resolveNativeSourceEdgePlan(split,{},physical,error)&&physical.components.size()==3);
    }
    check("source_PCM_and_OTTO_never_rewritten",sourceHash==juce::SHA256(source).toHexString()&&manifestHash==juce::SHA256(manifest).toHexString());
    // Legacy raw-O evidence belongs to the complete exact variant, whereas
    // activation belongs ONLY to the actual selected IDs (including singles).
    const auto rawRoot=root.getChildFile("raw-legacy"),rawSource=rawRoot.getChildFile("a_b_c.wav"),rawManifest=rawRoot.getChildFile("otto.ini");
    check("raw_manager_fixture",rawRoot.createDirectory().wasOk()&&source.copyFileTo(rawSource));
    auto rawRows=raw;rawRows[1].overlapSeconds=rawRows[2].overlapSeconds=0;
    OttoMaterialIO::Document rawDocument;rawDocument.audio={{rows.front().materialAudioId,rawSource.getFileName(),rawRows}};
    check("raw_manager_complete_overlap_variant",OttoMaterialIO::saveDocument(rawManifest,rawDocument,error)&&assetManager->diagnosticRegisterFolder(rawRoot)>=0);
    const auto rawHash=juce::SHA256(rawManifest).toHexString();
    for(const auto selection:{Selection{"raw_abc",0,3,true},Selection{"raw_ab",0,2,true},Selection{"raw_bc",1,2,true},Selection{"raw_a",0,1,true},Selection{"raw_b_zeroOwnO",1,1,true}})
    {
        project.resetDocument(seed);project.dispatchPendingMessages();selectedTrackId=track.id;pianoRoll.setFocusedTrack(track.id);audio.setPosition(0);resized();
        juce::StringArray ids;for(int i=0;i<selection.count;++i)ids.add(rawRows[std::size_t(selection.first+i)].materialRegionId);
        const auto payload=assetManager->pronunciationDragDescription(rawSource,selection.count==3?juce::StringArray{}:ids,rows.front().materialAudioId);
        juce::DragAndDropTarget::SourceDetails drop(payload,assetManager.get(),pianoViewport.getPosition()+juce::Point<int>(58,40));itemDropped(drop);closeWarnings();
        const auto data=project.snapshot();const auto prefix=juce::String(selection.name)+"_";
        const bool inserted=data.tracks.size()==1&&data.tracks[0].clips.size()==1&&data.tracks[0].clips[0].notes.size()==std::size_t(selection.count);
        check(prefix+"actual_manager_drag_placement",inserted);check(prefix+"analysis_completed",wait([&]{return pendingNativeAnalyses==0;},180000));if(!inserted)continue;
        const auto& clip=data.tracks[0].clips[0];bool optIn=true;
        for(const auto& note:clip.notes)optIn&=note.nativePronunciationEdges&&note.nativePronunciationEdges->provenance=="selected-overlap-usage-v1";
        NativeSourceEdgePlan plan;bool mask=resolveNativeSourceEdgePlan(data.tracks[0],{},plan,error)&&plan.components.size()==1&&plan.members.size()==std::size_t(selection.count);
        for(std::size_t i=0;mask&&i<plan.members.size();++i)mask&=plan.members[i].exposedEntry==(i==0)&&plan.members[i].exposedExit==(i+1==plan.members.size());
        check(prefix+"project_only_profile_and_actual_outer_masks",optIn&&mask);
        auto capture=data.tracks[0];capture.clips[0].startSeconds=0;ProjectData engineData;engineData.tracks={capture};AudioEngine engine(false);engine.syncProject(engineData);
        NativePronunciationEdges entry,exit;bool gains=defaultNativePronunciationEdges(rawRows[std::size_t(selection.first)],entry,error)
            &&defaultNativePronunciationEdges(rawRows[std::size_t(selection.first+selection.count-1)],exit,error);
        for(int i=0;gains&&i<juce::roundToInt(clip.durationSeconds*48000);++i)
        {
            const auto t=i/48000.0,sourceTime=t+clip.sourceOffsetSeconds;float expected=1;
            if(entry.entryToSourceSeconds>entry.entryFromSourceSeconds)expected*=smooth((sourceTime-entry.entryFromSourceSeconds)/(entry.entryToSourceSeconds-entry.entryFromSourceSeconds));
            if(exit.exitToSourceSeconds>exit.exitFromSourceSeconds)expected*=smooth((exit.exitToSourceSeconds-sourceTime)/(exit.exitToSourceSeconds-exit.exitFromSourceSeconds));
            const auto actual=engine.diagnosticNativeSourceEdgeGain(capture.clips[0].id,t);gains&=actual&&std::abs(*actual-expected)<2.e-6f;
        }
        check(prefix+"runtime_gain_uses_only_actual_outer_edges",gains);
        check(prefix+"single_undo",project.undo()&&!project.canUndo()&&project.snapshot().tracks[0].clips.empty());
    }
    check("raw_overlap_project_optin_never_writes_OTTO",juce::SHA256(rawManifest).toHexString()==rawHash);
    return finish();
#endif
}
}
