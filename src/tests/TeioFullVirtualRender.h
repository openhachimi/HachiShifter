#pragma once
#include "../MelodyneImportOptimization.h"
#include "../backend/MelodyneImporter.h"
#include "../AudioEngine.h"
#include <iostream>
namespace hachi
{
// Full input, all owners/notes/tracks retained. Diagnostics explicitly select
// WORLD; this is NOT an implicit NSF fallback or a short excerpt acceptance.
inline bool renderTeioFullVirtualProject(const juce::File& source,const juce::File& output,bool automaticPitch,bool virtualConnections=true,bool disableGroups=false)
{
    if(output.createDirectory().failed())return false;juce::String error,log;const auto start=juce::Time::getMillisecondCounterHiRes();
    const auto stage=[&](const juce::String& text){const auto line=juce::String(juce::Time::getMillisecondCounterHiRes()-start,1)+"ms | "+text;std::cout<<line<<std::endl;log+=line+"\n";output.getChildFile("stages.txt").replaceWithText(log);};
    const auto fail=[&](const juce::String& text){stage("FAILED: "+text);return false;};
    stage("import full original MPD; no recursive media search");backend::MelodyneImportOptions importOptions;importOptions.recursiveMediaSearch=false;
    auto imported=backend::MelodyneImporter::importProject(source,error,{},importOptions);if(!imported)return fail(error);if(!imported->missingFiles.isEmpty())return fail("Missing media: "+imported->missingFiles.joinIntoString(" | "));
    const auto counts=[](const ProjectData& data){std::pair<int,int> count{};for(const auto& track:data.tracks)for(const auto& clip:track.clips){++count.first;count.second+=int(clip.notes.size());}return count;};
    const auto originalCounts=counts(imported->project);const auto originalTracks=imported->project.tracks.size();
    stage("import complete tracks="+juce::String(int(originalTracks))+" owners="+juce::String(originalCounts.first)+" notes="+juce::String(originalCounts.second));
    std::map<juce::String,juce::String> sourceHashes;
    for(const auto& track:imported->project.tracks)for(const auto& clip:track.clips)if(clip.sourceFile.existsAsFile()&&!sourceHashes.count(clip.sourceFile.getFullPathName()))sourceHashes.emplace(clip.sourceFile.getFullPathName(),juce::SHA256(clip.sourceFile).toHexString());
    const auto originalProjectHash=juce::SHA256(source).toHexString();stage("captured all original source SHA256 values="+juce::String(int(sourceHashes.size())));
    MelodyneImportOptimizationOptions optimization;optimization.virtualConnections=virtualConnections;optimization.virtualPitch=automaticPitch;
    stage("prepare source identity; consonant merge OFF");const auto merged=prepareMelodyneImportOptimization(imported->project,imported->continuousSourceJoins,optimization);
    if(merged.merged!=0)return fail("Unexpected merge while disabled");
    stage("convert entire project IN MEMORY; publishAnnotations=false");juce::StringArray warnings;
    SampleSettings::convertMelodyneProject(imported->project,warnings,true,false);stage("conversion complete");
    for(auto& track:imported->project.tracks)if(trackShowsAllNativeRegions(track)&&!track.accompaniment)track.pitchAlgorithm=PitchAlgorithm::world;
    ProjectModel originalFacts;originalFacts.resetDocument(imported->project);const auto beforeRoutes=originalFacts.contentFingerprint();
    stage("build full virtual routes; WORLD EXPLICIT; virtual="+juce::String(virtualConnections?"ON":"OFF")+" auto pitch="+juce::String(automaticPitch?"ON":"OFF"));
    const auto report=finishMelodyneImportOptimization(imported->project,imported->continuousSourceJoins,optimization);warnings.addArray(report.skipped);
    stage("routes complete actions="+juce::String(report.created)+" groups="+juce::String(int(imported->project.nativeVirtualGroups.size())));
    auto facts=imported->project;facts.nativeVirtualGroups.clear();ProjectModel routedFacts;routedFacts.resetDocument(facts);
    if(routedFacts.contentFingerprint()!=beforeRoutes)return fail("Route creation changed original source/music/envelope facts");
    stage("all authoring facts unchanged after removing ONLY the newly added virtual groups");
    if(disableGroups)for(auto& group:imported->project.nativeVirtualGroups)group.enabled=false;
    for(const auto& group:imported->project.nativeVirtualGroups)for(const auto& track:imported->project.tracks)if(track.id==group.trackId)
    {
        NativeVirtualConnectionPlan geometry;juce::String issue;if(!resolveNativeVirtualGroup(track,group,geometry,issue))continue;
        const auto pitch=planNativeVirtualPitch(track,group,geometry);if(!pitch.conflictingWindows)continue;
        stage("CONFLICT group="+group.id+" members="+juce::String(int(group.orderedNoteIds.size())));
        for(const auto& segment:pitch.segments)if(!segment.points.empty())stage("window "+segment.groupId+" from="+juce::String(segment.points.front().timeSeconds,9)+" to="+juce::String(segment.points.back().timeSeconds,9)+" manual="+juce::String(segment.manual?1:0));
        for(const auto& member:geometry.members)stage("member "+member.noteId+" file="+member.source.getFileName()+" support="+juce::String(member.from,9)+".."+juce::String(member.to,9));
    }
    if(counts(imported->project)!=originalCounts||imported->project.tracks.size()!=originalTracks)return fail("Full project membership changed");
    ProjectModel model;model.resetDocument(imported->project);stage("save full independent diagnostic project");if(!model.save(output.getChildFile("full-teio.hjpx"),error))return fail(error);
    AudioEngine engine(false);engine.prepareToPlay(512,48000);stage("syncProject BEGIN (all owners; request trace active if configured)");engine.syncProject(imported->project);stage("syncProject END");
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+180000.;int last=-1;
    while(juce::Time::getMillisecondCounterHiRes()<deadline)
    {
        const auto p=engine.renderProgress();if(!p||*p>=.999)break;
        const int percent=int(*p*100);if(percent!=last){last=percent;stage("actual full-project synthesis progress="+juce::String(percent)+"%");}
        juce::Thread::sleep(20);
    }
    if(const auto p=engine.renderProgress();p&&*p<.999)return fail("Full-project render exceeded 180s synthesis deadline");
    stage("synthesis complete");if(engine.activeRenderWarnings().isNotEmpty())return fail(engine.activeRenderWarnings());
    stage("full WAV export BEGIN");const auto wave=output.getChildFile(!virtualConnections?"teio-full-no-optimizations.wav":disableGroups?"teio-full-virtual-disabled.wav":automaticPitch?"teio-full-auto-glide-on.wav":"teio-full-auto-glide-off.wav");if(!engine.exportWav(wave,error))return fail(error);stage("full WAV export END");
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wave));if(!reader||reader->lengthInSamples<=0)return fail("Empty/unreadable full export");
    double end=0;for(const auto& track:imported->project.tracks)for(const auto& clip:track.clips)end=std::max(end,clip.startSeconds+clip.durationSeconds);
    const auto length=reader->lengthInSamples/reader->sampleRate;if(length+1./reader->sampleRate<end)return fail("Export was truncated before the last project owner");
    juce::AudioBuffer<float> block(2,8192);float peak=0;bool finite=true;
    for(juce::int64 offset=0;offset<reader->lengthInSamples;offset+=8192)
    {
        const int count=int(std::min<juce::int64>(8192,reader->lengthInSamples-offset));block.clear();if(!reader->read(&block,0,count,offset,true,true))return fail("Full export read-back failed");
        for(int channel=0;channel<2;++channel)for(int sample=0;sample<count;++sample){const auto value=block.getSample(channel,sample);finite&=std::isfinite(value);peak=std::max(peak,std::abs(value));}
    }
    if(!finite||peak<.001f)return fail("Full export contains invalid or silent PCM");
    stage("SUCCESS full WORLD export seconds="+juce::String(length,6)+" sampleRate="+juce::String(reader->sampleRate,0)+" peak="+juce::String(peak,6)+" owners="+juce::String(originalCounts.first)+" notes="+juce::String(originalCounts.second));
    juce::Array<juce::var> verified;
    for(const auto& [path,hash]:sourceHashes)
    {
        const bool unchanged=juce::SHA256(juce::File(path)).toHexString()==hash;auto* row=new juce::DynamicObject;row->setProperty("path",path);row->setProperty("sha256",hash);row->setProperty("unchanged",unchanged);verified.add(juce::var(row));
        if(!unchanged)return fail("Original source bytes changed: "+path);
    }
    if(juce::SHA256(source).toHexString()!=originalProjectHash)return fail("Original MPD bytes changed");
    output.getChildFile("all-original-source-sha256.json").replaceWithText(juce::JSON::toString(juce::var(verified),true));
    stage("original MPD plus ALL "+juce::String(int(sourceHashes.size()))+" unique source WAV SHA256 values unchanged");
    stage("automatic glide effectiveness remains UNACCEPTED until full off/on comparison and user listening");return true;
}
}
