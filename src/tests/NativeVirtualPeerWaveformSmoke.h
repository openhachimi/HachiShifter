#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualRenderPlan.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualPeerWaveformSmoke(const juce::File& folder)
{
    if(folder.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    for(int i=0;i<3;++i)
    {
        const auto file=folder.getChildFile("source"+juce::String(i)+".wav");juce::AudioBuffer<float> pcm(2,96000);
        for(int s=0;s<96000;++s){pcm.setSample(0,s,i==1?.125f:.25f);pcm.setSample(1,s,i==1?-.25f:-.125f);}
        auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,96000))return false;
    }
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    for(int i=0;i<3;++i)
    {
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=folder.getChildFile("source"+juce::String(i)+".wav");clip.startSeconds=i==0?0:i==1?.8:2.;clip.durationSeconds=clip.sourceDurationSeconds=1.;clip.sourceTimeMap={{0,0},{1.,1.}};
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=1.;note.midiNote=note.sourceMidiCenter=57+float(i*7);note.sourcePitchMeasured=true;for(int f=0;f<=200;++f)note.contour.push_back({f*.005,0,0,true});clip.notes={note};track.clips.push_back(clip);
    }
    seed.tracks={track};NativeVirtualGroup group;group.id="virtual";group.trackId=track.id;group.orderedNoteIds={"note0","note1"};group.automaticPitch=true;group.pitchDurationSeconds=.2;seed.nativeVirtualGroups={group};
    const auto hash=[](const ProjectData& data,int index){return AudioEngine::nativeClipWaveformHash(data.tracks[0].clips[std::size_t(index)],data.tracks[0],&data.nativeVirtualGroups);};
    const auto base=hash(seed,0);auto changed=seed;changed.tracks[0].clips[1].notes[0].midiNote+=3;
    check("peer_transpose_invalidates_this_owners_waveform",hash(changed,0)!=base);
    changed=seed;changed.tracks[0].clips[1].startSeconds+=.05;check("peer_target_move_invalidates_this_owners_waveform",hash(changed,0)!=base);
    changed=seed;changed.tracks[0].clips[1].sourceTimeMap={{0,0},{.5,.4},{1.,1.}};check("peer_source_clock_edit_invalidates_this_owners_waveform",hash(changed,0)!=base);
    changed=seed;changed.tracks[0].clips[1].sourceFile=folder.getChildFile("source2.wav");check("peer_recording_replacement_invalidates_this_owners_waveform",hash(changed,0)!=base);
    const auto peerFile=seed.tracks[0].clips[1].sourceFile;const auto originalStamp=peerFile.getLastModificationTime();
    const bool stampUpdated=peerFile.setLastModificationTime(juce::Time(originalStamp.toMilliseconds()+3000));
    const bool stampChanged=hash(seed,0)!=base;peerFile.setLastModificationTime(originalStamp);
    check("peer_recording_file_stamp_invalidates_this_owners_waveform",stampUpdated&&stampChanged);
    changed=seed;changed.tracks[0].clips[2].notes[0].midiNote+=2;changed.tracks[0].clips[2].startSeconds+=.2;
    check("unrelated_same_track_note_does_not_invalidate_virtual_waveform",hash(changed,0)==base);
    auto off=seed;off.nativeVirtualGroups[0].enabled=false;auto plain=off;plain.nativeVirtualGroups.clear();check("disabled_groups_keep_the_exact_legacy_waveform_hash",hash(off,0)==hash(plain,0));
    // One compound parent is represented differently by the UI and renderer;
    // dependencies must nevertheless be the same absolute physical facts.
    auto compound=seed;ClipData parent;parent.id="assembly";parent.durationSeconds=3.;parent.parts={track.clips[0],track.clips[2]};parent.notes={track.clips[0].notes[0],track.clips[2].notes[0]};
    parent.notes[0].clipPartId=parent.parts[0].id;parent.notes[1].clipPartId=parent.parts[1].id;parent.notes[1].startSeconds=2.;for(auto& part:parent.parts)part.notes.clear();compound.tracks[0].clips={parent,track.clips[1]};
    auto renderView=compound,editorView=compound;expandProjectClipParts(renderView);expandProjectClipParts(editorView,true);
    check("peer_context_is_identical_for_compound_render_and_editor_views",hash(renderView,0)==hash(editorView,0));
    // Existing logical connection flags are intentionally suppressed ONLY in
    // the render projection. This must not make its wave invisible to the UI.
    auto playback=seed;playback.nativeVirtualGroups[0].automaticPitch=false;
    for(auto& clip:playback.tracks[0].clips)for(auto& note:clip.notes){note.nativeUnpitched=true;for(auto& p:note.contour)p.voiced=false;}
    playback.tracks[0].clips[0].notes[0].connectedToNext=true;playback.tracks[0].clips[0].glideConnectedToNext=true;
    playback.tracks[0].clips[1].notes[0].connectedToPrevious=true;playback.tracks[0].clips[1].glideConnectedFromPrevious=true;
    NativeVirtualConnectionPlan plan;resolveNativeVirtualGroup(playback.tracks[0],playback.nativeVirtualGroups[0],plan,error);
    AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(playback);const auto until=juce::Time::getMillisecondCounterHiRes()+10000.;
    while(juce::Time::getMillisecondCounterHiRes()<until){const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;juce::Thread::sleep(2);}
    engine.refreshNativeWaveforms();const auto waves=engine.nativeClipWaveforms();
    const auto find=[&](std::uint64_t key)->const NativeRenderedPeaks*{if(waves)for(const auto& wave:*waves)if(wave.audioHash==key)return wave.peaks.get();return nullptr;};
    const auto* a=find(hash(playback,0));const auto* b=find(hash(playback,1));const auto* unrelated=find(hash(playback,2));
    check("actual_engine_waveform_hashes_match_authoring_despite_suppressed_flags",a&&b&&unrelated);
    check("only_group_owners_publish_route_weighted_peaks",a&&b&&a->virtualContributionsApplied&&b->virtualContributionsApplied&&unrelated&&!unrelated->virtualContributionsApplied);
    double maxError=0;bool measured=a&&b;
    for(int owner=0;measured&&owner<2;++owner)
    {
        const auto* actual=owner?b:a;juce::AudioBuffer<float> raw(2,48000);
        for(int s=0;s<48000;++s){raw.setSample(0,s,owner?.125f:.25f);raw.setSample(1,s,owner?-.25f:-.125f);}
        const auto originSamples=owner?38400:0;
        const auto expected=measureWeightedNativeRenderedPeaks(raw,48000,[&](int sample){return nativeVirtualMemberContribution(plan,std::size_t(owner),(originSamples+sample)/48000.);},true);
        measured&=actual->sampleCount==expected->sampleCount&&actual->minima.size()==expected->minima.size();
        if(measured)for(std::size_t bucket=0;bucket<expected->minima.size();++bucket){maxError=std::max(maxError,double(std::abs(actual->minima[bucket]-expected->minima[bucket])));maxError=std::max(maxError,double(std::abs(actual->maxima[bucket]-expected->maxima[bucket])));}
    }
    std::cout<<"route_weighted_peak_max_error="<<maxError<<std::endl;check("every_actual_stereo_peak_bucket_includes_the_same_route_partition_as_playback",measured&&maxError<2.e-6);
    const auto published=engine.nativeClipWaveforms();engine.refreshNativeWaveforms();check("unchanged_refresh_reuses_the_existing_peak_snapshot",engine.nativeClipWaveforms()==published);
    auto isolated=playback;isolated.tracks[0].clips[2].notes[0].midiNote+=2;engine.syncProject(isolated);
    const auto isolatedUntil=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<isolatedUntil){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
    engine.refreshNativeWaveforms();bool reused=false;if(const auto updated=engine.nativeClipWaveforms())for(const auto& wave:*updated)if(wave.audioHash==hash(playback,0))reused=wave.peaks.get()==a;
    check("unrelated_edit_reuses_existing_weighted_peaks_across_loaded_clip_rebuild",reused);
    off=playback;off.nativeVirtualGroups[0].enabled=false;engine.syncProject(off);
    const auto offUntil=juce::Time::getMillisecondCounterHiRes()+10000.;while(juce::Time::getMillisecondCounterHiRes()<offUntil){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
    engine.refreshNativeWaveforms();const auto disabled=engine.nativeClipWaveforms();bool rawRestored=false;
    if(disabled)for(const auto& wave:*disabled)if(wave.audioHash==hash(off,0))rawRestored=wave.peaks&&!wave.peaks->virtualContributionsApplied;
    check("AB_disable_restores_unweighted_original_waveform_summary",rawRestored);
    folder.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
