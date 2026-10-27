#pragma once
#include "../NativeVirtualRenderPlan.h"
#include "../NativeVirtualGroupIO.h"
#include "../AudioEngine.h"
#include "../PianoRollComponent.h"
#include <iostream>
namespace hachi
{
inline bool runNativeVirtualChainSharedSmoke(const juce::File& output)
{
    if(output.createDirectory().failed())return false;bool ok=true;juce::String error,log;
    const auto check=[&](const char* name,bool pass){ok&=pass;log+=juce::String(name)+"="+(pass?"PASS":"FAIL")+" | "+error+"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    ProjectData seed;TrackData track;track.id="native";track.trackType=TrackType::hachi;track.pitchAlgorithm=PitchAlgorithm::world;track.compose=true;track.normalizeVolume=false;track.smoothOverlaps=false;
    std::vector<juce::String> ids;
    for(int i=0;i<7;++i)
    {
        const auto file=output.getChildFile("source"+juce::String(i)+".wav");
        {juce::AudioBuffer<float> pcm(1,48000);for(int s=0;s<pcm.getNumSamples();++s){const auto phase=juce::MathConstants<double>::twoPi*(180+i*19)*s/48000.;pcm.setSample(0,s,float(.1*std::sin(phase)+.03*std::sin(phase*2)));}auto stream=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,1,24,{},0));if(!writer||!writer->writeFromAudioSampleBuffer(pcm,0,pcm.getNumSamples()))return false;}
        ClipData clip;clip.id="owner"+juce::String(i);clip.sourceFile=file;clip.startSeconds=i*.3;clip.durationSeconds=clip.sourceDurationSeconds=.4;clip.sourceTimeMap={{0,0},{.4,.4}};
        NoteData note;note.id="note"+juce::String(i);note.durationSeconds=.4;note.midiNote=note.sourceMidiCenter=57+float(i);note.nativeUnpitched=true;
        for(int f=0;f<=80;++f)note.contour.push_back({f*.005,0,0,false});clip.notes={note};track.clips.push_back(clip);ids.push_back(note.id);
    }
    seed.tracks={track};ProjectModel model;model.resetDocument(seed);const auto original=model.contentFingerprint();
    check("seven_source_audio_link_uses_virtual_group_not_compound",model.linkNativeAudio(ids)&&model.snapshot().nativeVirtualGroups.size()==1&&model.snapshot().nativeVirtualGroups[0].sharedPitch&&model.snapshot().tracks[0].clips.size()==7&&model.snapshot().nativeConnections.empty());
    if(model.snapshot().nativeVirtualGroups.empty())return false;
    bool untouched=true;for(const auto& clip:model.snapshot().tracks[0].clips)untouched&=clip.parts.empty()&&!clip.nativeAudioLinked;
    check("all_original_owners_paths_and_note_ids_stay_independent",untouched&&model.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);
    model.undo();check("shared_mode_is_one_undoable_common_transaction",model.contentFingerprint()==original);model.redo();
    const auto group=model.snapshot().nativeVirtualGroups[0];const auto file=output.getChildFile("shared-chain.hjpx");ProjectModel cold;
    check("shared_mode_and_all_members_cold_roundtrip",model.save(file,error)&&cold.load(file,error)&&cold.snapshot().nativeVirtualGroups[0].sharedPitch&&cold.snapshot().nativeVirtualGroups[0].orderedNoteIds==ids);
    auto bad=nativeVirtualGroupTree(group);bad.setProperty("sharedPitch","not bool",nullptr);NativeVirtualGroup decoded;
    check("shared_mode_wrong_type_rejected",!nativeVirtualGroupFromTree(bad,decoded,error));
    NativeVirtualConnectionPlan geometry;check("long_chain_geometry_resolves",resolveNativeVirtualGroup(track,group,geometry,error)&&geometry.members.size()==7&&geometry.handovers.size()==6);
    bool partition=true,lastAudible=false;
    for(int sample=0;sample<120000;++sample)
    {
        const auto time=sample/48000.;double sum=0;bool active=false;
        for(std::size_t i=0;i<geometry.members.size();++i)
        {const auto& m=geometry.members[i];active|=time>=m.from&&time<m.to;sum+=nativeVirtualMemberContribution(geometry,i,time);}
        partition&=std::abs(sum-(active?1.:0.))<1.e-8;
        if(time>1.9&&time<2.1)lastAudible|=nativeVirtualMemberContribution(geometry,6,time)>.9;
    }
    check("all_seven_weights_partition_actual_support_without_truncation",partition&&lastAudible);
    auto many=seed;many.tracks[0].clips.clear();std::vector<juce::String> manyIds;
    for(int i=0;i<40;++i){auto clip=track.clips[std::size_t(i%7)];clip.id="many-owner"+juce::String(i);clip.startSeconds=i*.3;clip.notes[0].id="many-note"+juce::String(i);manyIds.push_back(clip.notes[0].id);many.tracks[0].clips.push_back(clip);}
    ProjectModel manyModel;manyModel.resetDocument(many);
    check("no_hidden_32_member_limit",manyModel.createNativeVirtualConnection(manyIds,false,error).isNotEmpty()&&manyModel.snapshot().nativeVirtualGroups[0].orderedNoteIds.size()==40);
    // Shared pitch is derived using the SAME established sharedPitchLines
    // service. Input note curves may be authored: the explicit link preserves
    // and shares them instead of treating them as an automatic-glide overwrite.
    auto voiced=seed;for(std::size_t i=0;i<voiced.tracks[0].clips.size();++i){auto& clip=voiced.tracks[0].clips[i];clip.startSeconds=i*.4;clip.notes[0].nativeUnpitched=false;for(auto& p:clip.notes[0].contour)p.voiced=true;}
    ProjectModel voice;voice.resetDocument(voiced);const bool linked=voice.linkNativeAudio(ids);const auto shared=voice.snapshot().nativeVirtualGroups[0];
    const auto lines=nativeVirtualSharedPitchLines(voiced.tracks[0],shared);const auto* line=lines.memberFor(ids.front());
    ProjectData projected;std::vector<NativeVirtualRenderGroup> rendered;const auto prepared=prepareNativeVirtualRender(voice.snapshot(),projected,rendered,error);
    bool exact=linked&&prepared&&!rendered.empty()&&rendered[0].pitch.active&&line&&line->line;
    if(exact)for(const auto& point:rendered[0].pitch.points)exact&=std::abs(point.targetMidi-line->line->midiAt(point.timeSeconds))<1.e-5;
    check("shared_target_uses_existing_continuous_pitch_authority",exact);
    I18n strings;PianoRollComponent roll(voice,strings);roll.setFocusedTrack(track.id);roll.setTool(PianoRollComponent::Tool::points);roll.setBounds(0,0,1100,800);roll.diagnosticRefresh();
    bool displayed=line&&line->line;
    for(const auto& clip:voiced.tracks[0].clips)
    {const auto time=clip.startSeconds+.2;const auto shown=roll.diagnosticPitchLineAt(clip.notes[0].id,time);displayed&=shown&&line&&line->line&&std::abs(*shown-line->line->midiAt(time))<1.e-4;}
    check("real_piano_roll_uses_the_same_shared_virtual_line",displayed);
    auto harmony=voiced;harmony.tracks[0].clips[2].startSeconds=.55;
    NativeVirtualConnectionPlan harmonyGeometry;const auto harmonyResolved=resolveNativeVirtualGroup(harmony.tracks[0],shared,harmonyGeometry,error);
    const auto unsafe=harmonyResolved?planNativeVirtualPitch(harmony.tracks[0],shared,harmonyGeometry):NativeVirtualPitchPlan{};
    check("shared_subchain_never_lends_its_pitch_to_an_independent_overlapping_voice",harmonyResolved&&!unsafe.active&&!unsafe.issue.isEmpty());
    const auto render=[&](const ProjectData& data,juce::AudioBuffer<float>& result)
    {
        AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);const auto deadline=juce::Time::getMillisecondCounterHiRes()+20000;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(2);}
        if(const auto p=engine.renderProgress();p&&*p<.999)return false;
        if(!engine.activeRenderWarnings().isEmpty()){error=engine.activeRenderWarnings();return false;}
        result.setSize(2,120000);result.clear();engine.setPosition(0);engine.play();for(int offset=0;offset<120000;offset+=257)engine.getNextAudioBlock({&result,offset,std::min(257,120000-offset)});engine.stop();return true;
    };
    // Route-only PCM fixture: all original masks stay UV, while the test's
    // tone source lets us measure exact per-source contribution at the mixer.
    ProjectModel routing;routing.resetDocument(seed);const auto routeId=routing.createNativeVirtualConnection(ids,false,error);
    std::array<juce::AudioBuffer<float>,7> stems;juce::AudioBuffer<float> mixed;bool live=routeId.isNotEmpty()&&render(routing.snapshot(),mixed);
    for(int i=0;i<7;++i){auto one=seed;one.tracks[0].clips={track.clips[std::size_t(i)]};live&=render(one,stems[std::size_t(i)]);}
    double maximum=0;
    if(live)for(int s=0;s<120000;++s)for(int ch=0;ch<2;++ch){double expected=0;for(int i=0;i<7;++i)expected+=nativeVirtualMemberContribution(geometry,std::size_t(i),s/48000.)*stems[std::size_t(i)].getSample(ch,s);maximum=std::max(maximum,std::abs(mixed.getSample(ch,s)-expected));}
    std::cout<<"seven_source_PCM_max_difference="<<maximum<<std::endl;
    check("real_audio_callback_routes_every_member_including_indices_over_four",live&&maximum<2.e-6);
    output.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
