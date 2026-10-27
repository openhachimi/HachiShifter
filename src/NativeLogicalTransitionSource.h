#pragma once
#include "NativeLogicalTransitionIO.h"
#include "NativeNoteTiming.h"
#include "SampleSettings.h"
#include <array>
#include <map>
#include <set>
namespace hachi
{
// One authoring interpretation shared by importer sidecars and explicit library
// registration. Never apply a full transition shape separately to physical halves.
inline bool nativeLogicalTransitionSourceRow(const NativeLogicalTransition& annotation,
    SampleRegionSetting& output,juce::String& error)
{
    NativeLogicalTransition checked;if(!nativeLogicalTransitionFromTree(nativeLogicalTransitionTree(annotation),checked,error))return false;
    SampleRegionSetting row;row.name="-";row.regionStartSeconds=annotation.sourceFromSeconds;row.regionEndSeconds=annotation.sourceToSeconds;
    row.pronunciationKind=PronunciationKind::transition;row.nativeTransition=NativeTransitionShape{1,annotation.middleSourceSeconds,"melodyne-two-piece-source-cut"};
    row.provenance="identified-logical-transition";row.confidence=annotation.confidence;
    if(!applyPronunciationTiming(row,pronunciationTimingFor(row),error))return false;
    NativePronunciationEdges edges;if(!defaultNativeTransitionEdges(row,edges,error))return false;
    row.nativePronunciationEdges=edges;output=std::move(row);return true;
}
inline bool resolveNativeLogicalTransitionSource(const ProjectData& project,const NativeLogicalTransition& annotation,
    juce::File& source,juce::String& error)
{
    struct Endpoint{const ClipData* parent=nullptr;const ClipData* owner=nullptr;const NoteData* note=nullptr;double offset=0;};
    std::array<Endpoint,2> endpoints;
    for(std::size_t i=0;i<2;++i)
    {
        const auto id=i?annotation.evidence.backNoteId:annotation.evidence.frontNoteId;auto& found=endpoints[i];
        for(const auto& track:project.tracks)for(const auto& parent:track.clips)for(const auto& note:parent.notes)if(note.id==id)
        {if(found.note){error="Ambiguous logical transition physical member";return false;}found.parent=&parent;found.note=&note;}
        if(!found.note){error="Missing logical transition physical member";return false;}
        found.owner=found.parent->parts.empty()?found.parent:nullptr;
        for(const auto& part:found.parent->parts)if(part.id==found.note->clipPartId)
        {if(found.owner){error="Ambiguous logical transition physical owner";return false;}found.owner=&part;found.offset=part.startSeconds;}
        if(!found.owner){error="Missing logical transition source owner";return false;}
    }
    if(endpoints[0].owner->sourceFile!=endpoints[1].owner->sourceFile||endpoints[0].owner->sourceFile==juce::File{})
    {error="Logical transition source owners disagree";return false;}
    for(std::size_t i=0;i<2;++i)
    {
        const auto& endpoint=endpoints[i];auto note=*endpoint.note;note.startSeconds-=endpoint.offset;
        const auto row=nativeNoteSourceTiming(note,*endpoint.owner,nativeClipClock(*endpoint.owner));
        const auto from=i?annotation.middleSourceSeconds:annotation.sourceFromSeconds;
        const auto to=i?annotation.sourceToSeconds:annotation.middleSourceSeconds;
        if(std::abs(row.regionStartSeconds-from)>1.e-6||std::abs(row.regionEndSeconds-to)>1.e-6)
        {error="Logical transition no longer matches its exact physical source cut";return false;}
    }
    source=endpoints[0].owner->sourceFile;return true;
}
// Rewrite only fresh importer annotation rows, never the project or original
// audio. Actual atomic file publication remains SampleSettings/OttoMaterialIO.
inline int coalesceNativeLogicalTransitionSourceRows(const ProjectData& project,
    std::map<juce::String,std::vector<SampleRegionSetting>>& rows,juce::StringArray& warnings)
{
    int changed=0;std::set<juce::String> processed;
    for(const auto& annotation:project.logicalTransitions)
    {
        juce::File source;juce::String error;SampleRegionSetting complete;
        if(!nativeLogicalTransitionSourceRow(annotation,complete,error)||!resolveNativeLogicalTransitionSource(project,annotation,source,error))
        {warnings.add(error);continue;}
        const auto file=rows.find(source.getFullPathName());if(file==rows.end())continue;
        auto& current=file->second;const auto equal=[](double a,double b){return std::abs(a-b)<=1.e-6;};
        const auto key=source.getFullPathName()+"|"+juce::String(annotation.sourceFromSeconds,12)+"|"+juce::String(annotation.sourceToSeconds,12);
        if(!processed.insert(key).second)continue;
        bool ambiguous=false;
        for(const auto& other:project.logicalTransitions)
        {
            if(other.evidence.logicalId==annotation.evidence.logicalId||!equal(other.sourceFromSeconds,annotation.sourceFromSeconds)||!equal(other.sourceToSeconds,annotation.sourceToSeconds)||equal(other.middleSourceSeconds,annotation.middleSourceSeconds))continue;
            juce::File otherSource;juce::String ignored;if(resolveNativeLogicalTransitionSource(project,other,otherSource,ignored)&&otherSource==source)ambiguous=true;
        }
        if(ambiguous){warnings.add("Conflicting logical transition middle cuts require independent source variants: "+source.getFileName());continue;}
        const auto front=[&](const auto& row){return equal(row.regionStartSeconds,annotation.sourceFromSeconds)&&equal(row.regionEndSeconds,annotation.middleSourceSeconds);};
        const auto back=[&](const auto& row){return equal(row.regionStartSeconds,annotation.middleSourceSeconds)&&equal(row.regionEndSeconds,annotation.sourceToSeconds);};
        if(std::none_of(current.begin(),current.end(),front)||std::none_of(current.begin(),current.end(),back))continue;
        std::erase_if(current,[&](const auto& row){return front(row)||back(row);});
        const auto exists=std::any_of(current.begin(),current.end(),[&](const auto& row){return row.pronunciationKind==PronunciationKind::transition&&row.nativeTransition&&equal(row.regionStartSeconds,annotation.sourceFromSeconds)&&equal(row.regionEndSeconds,annotation.sourceToSeconds)&&equal(row.nativeTransition->boundarySourceSeconds,annotation.middleSourceSeconds);});
        if(!exists)current.push_back(std::move(complete));++changed;
    }
    return changed;
}
}
