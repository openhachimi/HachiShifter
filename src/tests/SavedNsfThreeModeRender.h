#pragma once
#include "../AudioEngine.h"
#include "../NativeVirtualAutoConnect.h"
#include "../NativeVirtualRenderPlan.h"
#include "../backend/NsfHifiganRenderer.h"
#include <iostream>
namespace hachi
{
// Requested saved HJPX, never an MPD substitute. All three modes are derived
// from ONE loaded authoring document; only optimization state differs.
inline bool renderSavedNsfThreeModes(const juce::File& source,const juce::File& modelDirectory,const juce::File& output)
{
    if(output.createDirectory().failed())return false;juce::String error,log;const auto started=juce::Time::getMillisecondCounterHiRes();
    const auto stage=[&](const juce::String& text){const auto row=juce::String(juce::Time::getMillisecondCounterHiRes()-started,1)+"ms | "+text;std::cout<<row<<std::endl;log+=row+"\n";output.getChildFile("stages.txt").replaceWithText(log);};
    const auto fail=[&](const juce::String& text){stage("FAILED: "+text);return false;};
    const auto sourceHash=juce::SHA256(source).toHexString();
    if(!backend::NsfHifiganRenderer::modelAvailable(modelDirectory))return fail("Requested NSF model is unavailable; no fallback is permitted");
    std::map<juce::String,juce::String> modelHashes;
    for(const auto entry:juce::RangedDirectoryIterator(modelDirectory,true,"*",juce::File::findFiles))
    {const auto file=entry.getFile();modelHashes.emplace(file.getFullPathName(),juce::SHA256(file).toHexString());}
    stage("NSF model available: "+modelDirectory.getFullPathName()+"; files="+juce::String(int(modelHashes.size())));
    ProjectModel loaded;if(!loaded.load(source,error))return fail(error);if(!error.isEmpty())return fail("Project contains unresolved media: "+error);
    auto data=loaded.snapshot();
    for(const auto& track:data.tracks)if(track.compose&&!track.accompaniment)
    {loaded.setTrackPitchAlgorithm(track.id,PitchAlgorithm::nsfHifigan);loaded.setTrackRenderOrder(track.id,RenderOrder::stretchSpliceThenPitch);}
    data=loaded.snapshot();
    int ownerCount=0,noteCount=0;double expectedEnd=0;std::map<juce::String,juce::String> mediaHashes;
    for(const auto& track:data.tracks)for(const auto& parent:track.clips)
    {
        noteCount+=int(parent.notes.size());expectedEnd=std::max(expectedEnd,parent.startSeconds+parent.durationSeconds);
        for(const auto& clip:expandedClipParts(parent))
        {
            ++ownerCount;if(!clip.sourceFile.existsAsFile())return fail("Missing original media: "+clip.sourceFile.getFullPathName());
            if(!mediaHashes.count(clip.sourceFile.getFullPathName()))mediaHashes.emplace(clip.sourceFile.getFullPathName(),juce::SHA256(clip.sourceFile).toHexString());
        }
    }
    stage("loaded EXACT requested project="+data.name+" bpm="+juce::String(data.bpm)+" tracks="+juce::String(int(data.tracks.size()))+" owners="+juce::String(ownerCount)+" notes="+juce::String(noteCount)+" end="+juce::String(expectedEnd,9));
    auto baseline=data;for(auto& group:baseline.nativeVirtualGroups)group.enabled=false;
    auto connected=data;for(auto& group:connected.nativeVirtualGroups){group.enabled=true;group.automaticPitch=false;}
    std::vector<NativeSplitTransitionEvidence> joins;for(const auto& transition:data.logicalTransitions)joins.push_back(transition.evidence);
    const auto report=autoConnectNativeVirtualImport(connected,{true,false},joins);
    stage("conservative virtual discovery commits="+juce::String(report.created)+" groups="+juce::String(int(connected.nativeVirtualGroups.size()))+" skipped="+juce::String(report.skipped.size()));
    if(!report.skipped.isEmpty())output.getChildFile("connection-diagnostics.txt").replaceWithText(report.skipped.joinIntoString("\n"));
    auto pitched=connected;for(auto& group:pitched.nativeVirtualGroups)if(group.enabled)group.automaticPitch=true;
    // Exact proof that ON differs from OFF only in the requested automatic flag.
    auto comparable=pitched;for(std::size_t i=0;i<comparable.nativeVirtualGroups.size();++i)comparable.nativeVirtualGroups[i].automaticPitch=connected.nativeVirtualGroups[i].automaticPitch;
    ProjectModel offFacts,onFacts;offFacts.resetDocument(connected);onFacts.resetDocument(comparable);
    if(offFacts.contentFingerprint()!=onFacts.contentFingerprint())return fail("ON/OFF authoring state changed beyond automaticPitch flags");
    auto originalFacts=connected;originalFacts.nativeVirtualGroups=data.nativeVirtualGroups;ProjectModel musicFacts;musicFacts.resetDocument(originalFacts);
    if(musicFacts.contentFingerprint()!=loaded.contentFingerprint())return fail("Virtual discovery changed original source/music/envelope facts");
    stage("all source/music/envelope facts unchanged; modes 2/3 differ ONLY by automaticPitch");
    juce::Array<juce::var> outcomes;const std::array<juce::String,3> names{"01-all-optimizations-off","02-virtual-no-auto-glide","03-virtual-auto-glide"};
    AudioEngine engine(false);engine.prepareToPlay(512,48000);engine.setHifiganModelDirectory(modelDirectory);engine.setInferenceConfiguration(backend::InferenceBackend::cpu,-1);
    for(int mode=0;mode<3;++mode)
    {
        const auto& state=mode==0?baseline:mode==1?connected:pitched;const auto folder=output.getChildFile(names[std::size_t(mode)]);if(folder.createDirectory().failed())return fail("Cannot create output folder");
        ProjectModel own;own.resetDocument(state);if(!own.save(folder.getChildFile("render-project.hjpx"),error))return fail(error);
        ProjectData projection;std::vector<NativeVirtualRenderGroup> groups;if(!prepareNativeVirtualRender(state,projection,groups,error))return fail(error);
        const auto curves=nativeVirtualResolvedPitchCurves(groups,data.tracks.empty()?juce::String{}:data.tracks[0].id);
        stage(names[std::size_t(mode)]+" BEGIN pitch windows="+juce::String(int(curves.size()))+"; NSF-HiFiGAN CPU; stretchSpliceThenPitch; original stretch mode retained");
        const auto renderStart=juce::Time::getMillisecondCounterHiRes();engine.syncProject(state);
        const auto deadline=juce::Time::getMillisecondCounterHiRes()+900000.;int previous=-1;
        while(juce::Time::getMillisecondCounterHiRes()<deadline)
        {
            const auto progress=engine.renderProgress();if(!progress||*progress>=.999)break;
            const auto percent=int(*progress*100);if(percent!=previous){previous=percent;stage(names[std::size_t(mode)]+" actual synthesis="+juce::String(percent)+"%");}juce::Thread::sleep(25);
        }
        if(const auto p=engine.renderProgress();p&&*p<.999)return fail("Requested NSF synthesis exceeded 15min; last phase retained");
        if(engine.activeRenderWarnings().isNotEmpty())return fail(engine.activeRenderWarnings());
        const auto backends=engine.activeRenderBackends();stage(names[std::size_t(mode)]+" actual backend="+backends);
        if(!backends.containsIgnoreCase("nsf-hifigan")||backends.containsIgnoreCase("fallback")||backends.containsIgnoreCase("WORLD"))return fail("Actual backend did not satisfy NSF-only request: "+backends);
        const auto wave=output.getChildFile(names[std::size_t(mode)]+".wav");if(!engine.exportWav(wave,error))return fail(error);
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wave));if(!reader)return fail("Export is not readable");
        const double seconds=reader->lengthInSamples/reader->sampleRate;if(seconds+1./reader->sampleRate<expectedEnd)return fail("Export is truncated before the original end");
        juce::AudioBuffer<float> block(2,8192);float peak=0;double energy=0;juce::int64 samples=0;
        for(juce::int64 start=0;start<reader->lengthInSamples;start+=8192)
        {
            const int count=int(std::min<juce::int64>(8192,reader->lengthInSamples-start));block.clear();if(!reader->read(&block,0,count,start,true,true))return fail("Full exported PCM read failed");
            for(int channel=0;channel<2;++channel)for(int sample=0;sample<count;++sample){const auto value=block.getSample(channel,sample);if(!std::isfinite(value))return fail("Nonfinite exported PCM");peak=std::max(peak,std::abs(value));energy+=value*value;++samples;}
        }
        if(peak<.001f)return fail("NSF export is silent");
        auto* result=new juce::DynamicObject;result->setProperty("mode",names[std::size_t(mode)]);result->setProperty("file",wave.getFullPathName());result->setProperty("backend",backends);result->setProperty("renderOrder","stretchSpliceThenPitch");result->setProperty("pitchWindows",int(curves.size()));result->setProperty("frames",reader->lengthInSamples);result->setProperty("sampleRate",reader->sampleRate);result->setProperty("seconds",seconds);result->setProperty("peak",peak);result->setProperty("rms",std::sqrt(energy/std::max<juce::int64>(1,samples)));result->setProperty("elapsedMilliseconds",juce::Time::getMillisecondCounterHiRes()-renderStart);result->setProperty("sha256",juce::SHA256(wave).toHexString());outcomes.add(juce::var(result));
        output.getChildFile("render-results.json").replaceWithText(juce::JSON::toString(juce::var(outcomes),true));stage(names[std::size_t(mode)]+" COMPLETE seconds="+juce::String(seconds,9)+" peak="+juce::String(peak,6));
    }
    juce::Array<juce::var> integrity;
    for(const auto* hashes:{&mediaHashes,&modelHashes})for(const auto& [path,hash]:*hashes)
    {
        const bool unchanged=juce::SHA256(juce::File(path)).toHexString()==hash;auto* row=new juce::DynamicObject;row->setProperty("path",path);row->setProperty("sha256",hash);row->setProperty("unchanged",unchanged);integrity.add(juce::var(row));if(!unchanged)return fail("Original media/model changed: "+path);
    }
    if(juce::SHA256(source).toHexString()!=sourceHash)return fail("Original requested project changed");
    output.getChildFile("source-and-model-integrity.json").replaceWithText(juce::JSON::toString(juce::var(integrity),true));stage("SUCCESS all three complete NSF exports; original project/media/models unchanged; no source publication; no WORLD fallback");return true;
}
}
