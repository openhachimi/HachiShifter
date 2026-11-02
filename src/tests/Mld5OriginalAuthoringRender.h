#pragma once
#include "../AudioEngine.h"
#include "../backend/MelodyneImporter.h"
#include "../MelodyneImportOptimization.h"
#include <iostream>
namespace hachi
{
// Real authoring-state comparison: no +3 transposition, -12dB gain change,
// normalization override or disabling virtual groups to manufacture a pass.
inline bool renderMld5OriginalAuthoring(const juce::File& source,const juce::File& output)
{
    output.createDirectory();juce::String error,log;const auto hash=juce::SHA256(source).toHexString();
    const auto record=[&](const juce::String& text){log+=text+"\n";std::cout<<text<<std::endl;output.getChildFile("stages.txt").replaceWithText(log);};
    const auto fail=[&](const juce::String& why){record("FAILED: "+why);return false;};ProjectData data;
    if(source.hasFileExtension("mpd"))
    {
        backend::MelodyneImportOptions input;input.recursiveMediaSearch=false;auto imported=backend::MelodyneImporter::importProject(source,error,{},input);if(!imported)return fail(error);data=std::move(imported->project);
        MelodyneImportOptimizationOptions options;prepareMelodyneImportOptimization(data,imported->continuousSourceJoins,options);juce::StringArray warnings;SampleSettings::convertMelodyneProject(data,warnings,true,false);finishMelodyneImportOptimization(data,imported->continuousSourceJoins,options);
    }
    else{ProjectModel model;if(!model.load(source,error))return fail(error);data=model.snapshot();}
    for(const auto& group:data.nativeVirtualGroups)if(group.enabled)return fail("Original authoring contains a virtual source assembly not yet supported by MLD5; no group was silently disabled");
    std::map<juce::String,juce::String> media;int noteCount=0;
    for(auto& track:data.tracks)if(trackShowsAllNativeRegions(track)&&!track.accompaniment)
    {
        track.pitchAlgorithm=PitchAlgorithm::mld5;track.renderOrder=RenderOrder::processThenSplice;track.mergeContiguousSource=true;track.mld5RenderPolicyInitialized=true;
        for(const auto& parent:track.clips){noteCount+=int(parent.notes.size());for(const auto& owner:expandedClipParts(parent))if(owner.sourceFile.existsAsFile()){const auto path=owner.sourceFile.getFullPathName();if(!media.count(path))media[path]=juce::SHA256(owner.sourceFile).toHexString();}}
    }
    ProjectModel copy;copy.resetDocument(data);if(!copy.save(output.getChildFile("authoring-mld5-source.hjpx"),error))return fail(error);
    record("Original pitch/volume/normalization/joins/clocks retained; only MLD5+source-continuous mode selected in diagnostic copy; notes="+juce::String(noteCount));
    AudioEngine engine(false);engine.prepareToPlay(257,48000);engine.syncProject(data);const auto deadline=juce::Time::getMillisecondCounterHiRes()+180000.;
    while(juce::Time::getMillisecondCounterHiRes()<deadline){const auto p=engine.renderProgress();if(!p||*p>=.999)break;juce::Thread::sleep(10);}
    output.getChildFile("source-sessions.json").replaceWithText(juce::JSON::toString(engine.diagnosticMld5SourceSessions(),true));
    if(const auto p=engine.renderProgress();p&&*p<.999)return fail("Original-state MLD5 render timeout");if(const auto why=engine.activeRenderWarnings();why.isNotEmpty())return fail(why);
    const auto backend=engine.activeRenderBackends();record("backend="+backend+"; continuousSessions="+juce::String(engine.diagnosticMergedPhraseCount()));
    const auto wave=output.getChildFile("mld5-original-source.wav");if(!engine.exportWav(wave,error))return fail(error);
    juce::Array<juce::var> edges;for(const auto& track:data.tracks)
    {
        const auto clips=[&]{std::vector<ClipData> result;for(const auto& parent:track.clips){auto expanded=expandedClipParts(parent);result.insert(result.end(),expanded.begin(),expanded.end());}return result;}();
        for(const auto& left:clips)for(const auto& right:clips)if(left.id!=right.id&&left.sourceFile==right.sourceFile&&(left.glideConnectedToNext||(!left.notes.empty()&&left.notes.back().connectedToNext))&&(right.glideConnectedFromPrevious||(!right.notes.empty()&&right.notes.front().connectedToPrevious))&&std::abs(left.sourceOffsetSeconds+left.sourceDurationSeconds-right.sourceOffsetSeconds)<.01)
        {auto* row=new juce::DynamicObject();row->setProperty("leftClip",left.id);row->setProperty("rightClip",right.id);row->setProperty("leftNotes",left.notes.empty()?juce::String{}:left.notes.back().id);row->setProperty("rightNotes",right.notes.empty()?juce::String{}:right.notes.front().id);row->setProperty("sourceFile",left.sourceFile.getFullPathName());row->setProperty("leftEnd",left.startSeconds+left.durationSeconds);row->setProperty("rightStart",right.startSeconds);row->setProperty("classification","explicit author flags with source endpoint candidate; correlate session trace before claiming coverage");edges.add(juce::var(row));}
    }
    output.getChildFile("author-connection-candidates.json").replaceWithText(juce::JSON::toString(juce::var(edges),true));
    for(const auto& [path,before]:media)if(juce::SHA256(juce::File(path)).toHexString()!=before)return fail("Original source audio changed");if(juce::SHA256(source).toHexString()!=hash)return fail("Original project changed");
    record("Original source bytes unchanged; exported authoring-state WAV and per-clip session/rejection trace. Listening acceptance is separate.");return true;
}
}
