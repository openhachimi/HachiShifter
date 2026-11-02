#pragma once
#include "../AudioEngine.h"
#include "../backend/MelodyneImporter.h"
#include "../MelodyneImportOptimization.h"
#include <iostream>
namespace hachi
{
inline bool renderMld5RealOrders(const juce::File& source,const juce::File& output)
{
    output.createDirectory();juce::String error,log;const auto sourceHash=juce::SHA256(source).toHexString();
    const auto record=[&](const juce::String& message){log+=message+"\n";std::cout<<message<<std::endl;output.getChildFile("stages.txt").replaceWithText(log);};
    const auto fail=[&](const juce::String& why){record("FAILED: "+why);return false;};ProjectData original;
    if(source.hasFileExtension("mpd"))
    {
        backend::MelodyneImportOptions options;options.recursiveMediaSearch=false;auto imported=backend::MelodyneImporter::importProject(source,error,{},options);
        if(!imported)return fail(error);original=std::move(imported->project);MelodyneImportOptimizationOptions policy;
        prepareMelodyneImportOptimization(original,imported->continuousSourceJoins,policy);juce::StringArray warnings;SampleSettings::convertMelodyneProject(original,warnings,true,false);finishMelodyneImportOptimization(original,imported->continuousSourceJoins,policy);
    }
    else{ProjectModel model;if(!model.load(source,error))return fail(error);original=model.snapshot();}
    int disabled=0;for(auto& group:original.nativeVirtualGroups){disabled+=group.enabled;group.enabled=false;}
    record("Read-only real source="+source.getFullPathName()+"; diagnostic disables "+juce::String(disabled)+" unsupported virtual assemblies, retaining physical joins");
    std::map<juce::String,juce::String> media;double end=0;
    for(auto& track:original.tracks)
    {
        if(!trackShowsAllNativeRegions(track)||track.accompaniment)continue;
        track.pitchAlgorithm=PitchAlgorithm::mld5;track.mld5RenderPolicyInitialized=true;track.normalizeVolume=false;track.volume*=.25f;
        for(auto& parent:track.clips){end=std::max(end,parent.startSeconds+parent.durationSeconds);for(auto& note:parent.notes)note.midiNote+=3;for(const auto& owner:expandedClipParts(parent)){if(!owner.sourceFile.existsAsFile())return fail("Missing original recording: "+owner.sourceFile.getFullPathName());const auto path=owner.sourceFile.getFullPathName();if(!media.count(path))media[path]=juce::SHA256(owner.sourceFile).toHexString();}}
    }
    if(end<=0)return fail("No native audio in provided project");juce::Array<juce::var> results;
    const std::array<juce::String,3> labels{"01-per-note","02-splice-before-pitch","03-continuous-source"};int continuousGroups=0;
    for(int mode=0;mode<3;++mode)
    {
        auto data=original;for(auto& track:data.tracks)if(trackShowsAllNativeRegions(track)&&!track.accompaniment){track.renderOrder=mode==1?RenderOrder::stretchSpliceThenPitch:RenderOrder::processThenSplice;track.mergeContiguousSource=mode==2;}
        record(labels[std::size_t(mode)]+" BEGIN; equal -12dB diagnostic track gain; +3 semitone target edit; source clips/clocks unchanged");
        AudioEngine engine(false);engine.prepareToPlay(512,48000);engine.syncProject(data);const auto deadline=juce::Time::getMillisecondCounterHiRes()+180000.;
        while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(10);}
        if(const auto p=engine.renderProgress();p&&*p<.999)return fail("Real MLD5 order render timeout");
        if(const auto warnings=engine.activeRenderWarnings();warnings.isNotEmpty())return fail(warnings);
        const auto backendName=engine.activeRenderBackends();if(!backendName.contains(backend::Mld5Renderer::implementationId))return fail("Real render did not use recovered MLD5: "+backendName);
        const auto groups=engine.diagnosticMergedPhraseCount();if(mode==2)continuousGroups=groups;
        const auto wave=output.getChildFile(labels[std::size_t(mode)]+".wav");if(!engine.exportWav(wave,error))return fail(error);
        juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wave));if(!reader)return fail("Cannot cold-read exported MLD5 WAV");
        if(reader->lengthInSamples/reader->sampleRate+1./reader->sampleRate<end)return fail("Real MLD5 render truncated timeline");
        juce::AudioBuffer<float> block(2,8192);double energy=0,maxStep=0;float peak=0,previous=0;juce::int64 count=0;
        for(juce::int64 start=0;start<reader->lengthInSamples;start+=8192){const int n=int(std::min<juce::int64>(8192,reader->lengthInSamples-start));if(!reader->read(&block,0,n,start,true,true))return fail("Cannot decode rendered WAV");for(int i=0;i<n;++i){const auto sample=block.getSample(0,i);if(!std::isfinite(sample))return fail("Nonfinite MLD5 render");peak=std::max(peak,std::abs(sample));energy+=sample*sample;maxStep=std::max(maxStep,double(std::abs(sample-previous)));previous=sample;++count;}}
        if(peak<1.e-6f)return fail("Silent real MLD5 render");auto* row=new juce::DynamicObject();row->setProperty("mode",labels[std::size_t(mode)]);row->setProperty("backend",backendName);row->setProperty("continuousGroups",groups);row->setProperty("frames",reader->lengthInSamples);row->setProperty("rate",reader->sampleRate);row->setProperty("peak",peak);row->setProperty("rms",std::sqrt(energy/std::max<juce::int64>(1,count)));row->setProperty("globalMaxAdjacentSampleDelta",maxStep);row->setProperty("wav",wave.getFullPathName());results.add(juce::var(row));
        output.getChildFile("results.json").replaceWithText(juce::JSON::toString(juce::var(results),true));record(labels[std::size_t(mode)]+" COMPLETE; groups="+juce::String(groups)+"; peak="+juce::String(peak,8));
    }
    for(const auto& [path,hash]:media)if(juce::SHA256(juce::File(path)).toHexString()!=hash)return fail("Original recording changed: "+path);
    if(juce::SHA256(source).toHexString()!=sourceHash)return fail("Original project changed");
    record("Original project/media unchanged; no listening-quality claim from global PCM metrics alone; continuous groups="+juce::String(continuousGroups));
    return continuousGroups>0;
}
}
