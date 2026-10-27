#pragma once
#include "NativeVirtualGroupPlan.h"
namespace hachi
{
// Serializes ONLY selected peers of this group, using canonical ABSOLUTE score
// and source clocks. Parent, expanded render and read-only view representations
// therefore produce identical dependencies. No recursive hash/project clone.
template<class WriteNote>
inline void writeNativeVirtualWaveformPeers(juce::MemoryOutputStream& stream,const TrackData& track,
    const NativeVirtualGroup& group,WriteNote&& writeNote)
{
    stream.writeString("virtual-peer-context-v1");
    stream.writeString("virtual-single-target-v1");
    for(const auto& id:group.orderedNoteIds)
    {
        stream.writeString(id);bool found=false;
        for(const auto& parent:track.clips)for(const auto& original:parent.notes)if(original.id==id)
        {
            const ClipData* owner=parent.parts.empty()?&parent:nullptr;
            for(const auto& part:parent.parts)if(part.id==original.clipPartId)owner=&part;
            if(!owner)continue;found=true;
            const auto offset=parent.parts.empty()?0.:owner->startSeconds;
            const auto origin=parent.startSeconds+offset;
            stream.writeString(owner->sourceFile.getFullPathName());
            stream.writeInt64(owner->sourceFile.getSize());stream.writeInt64(owner->sourceFile.getLastModificationTime().toMilliseconds());
            stream.writeDouble(owner->sourceOffsetSeconds);stream.writeDouble(owner->sourceDurationSeconds);
            stream.writeDouble(origin+owner->audioStartSeconds);stream.writeDouble(owner->audioLength());
            std::vector<SourceTimePoint> clock;
            if(parent.parts.empty())clock=nativeClipClock(*owner);
            else
            {
                auto clockOwner=*owner;clockOwner.notes.clear();
                for(auto note:parent.notes)if(note.clipPartId==owner->id){note.startSeconds-=offset;clockOwner.notes.push_back(std::move(note));}
                clock=nativeClipClock(clockOwner);
            }
            stream.writeInt(int(clock.size()));
            for(const auto& point:clock){stream.writeDouble(origin+point.targetSeconds);stream.writeDouble(owner->sourceOffsetSeconds+point.sourceSeconds);}
            stream.writeInt(int(original.pronunciationKind));stream.writeBool(original.nativeTransition.has_value());
            if(original.nativeTransition){stream.writeInt(original.nativeTransition->version);stream.writeDouble(original.nativeTransition->boundarySourceSeconds);}
            writeNote(stream,original,parent.startSeconds);
        }
        stream.writeBool(found);
    }
    for(const auto* proof:nativeVirtualSplitProofs(group))
    {
        stream.writeString(proof->logicalId);stream.writeString(proof->frontNoteId);stream.writeString(proof->backNoteId);
        stream.writeString(proof->sourceObjectIdentity);stream.writeString(proof->frontElementIdentity);stream.writeString(proof->backElementIdentity);
        stream.writeBool(proof->joinsPitches);stream.writeBool(proof->joinsAmplitudes);stream.writeBool(proof->joinsPhases);stream.writeBool(proof->transitionIdentified);
    }
}
}
