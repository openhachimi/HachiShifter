#pragma once
#include "NativeVirtualPitchPlan.h"
#include "NativeLogicalTransitionIO.h"
#include <set>
#include <cmath>

namespace hachi
{
inline juce::ValueTree nativeVirtualGroupTree(const NativeVirtualGroup& group)
{
    juce::ValueTree tree("NativeVirtualGroup");tree.setProperty("version",group.version,nullptr);
    tree.setProperty("id",group.id,nullptr);tree.setProperty("trackId",group.trackId,nullptr);
    tree.setProperty("enabled",group.enabled,nullptr);tree.setProperty("automaticPitch",group.automaticPitch,nullptr);
    tree.setProperty("automaticallyCreated",group.automaticallyCreated,nullptr);
    if(group.sharedPitch)tree.setProperty("sharedPitch",true,nullptr);
    if(!group.coupledMovement)tree.setProperty("coupledMovement",false,nullptr);
    if(group.pitchCurveEncoding!=1)tree.setProperty("pitchCurveEncoding",group.pitchCurveEncoding,nullptr);
    tree.setProperty("pitchDurationSeconds",group.pitchDurationSeconds,nullptr);
    for(const auto& id:group.orderedNoteIds){juce::ValueTree member("Member");member.setProperty("noteId",id,nullptr);tree.addChild(member,-1,nullptr);}
    if(group.splitTransition)tree.addChild(nativeSplitTransitionEvidenceTree(*group.splitTransition),-1,nullptr);
    for(const auto& proof:group.extraSplitTransitions)tree.addChild(nativeSplitTransitionEvidenceTree(proof),-1,nullptr);
    if(!group.pitchCurve.empty())
    {
        tree.setProperty("pitchFromOffsetSeconds",group.pitchFromOffsetSeconds,nullptr);tree.setProperty("pitchToOffsetSeconds",group.pitchToOffsetSeconds,nullptr);
        for(const auto& p:group.pitchCurve)
        {
            juce::ValueTree point("VirtualPitchPoint");point.setProperty("u",p.timeSeconds,nullptr);point.setProperty("residual",p.targetMidi,nullptr);
            point.setProperty("shape",int(p.shape),nullptr);point.setProperty("x1",p.bezierX1,nullptr);point.setProperty("y1",p.bezierY1,nullptr);
            point.setProperty("x2",p.bezierX2,nullptr);point.setProperty("y2",p.bezierY2,nullptr);point.setProperty("precise",p.highPrecisionBezier,nullptr);tree.addChild(point,-1,nullptr);
        }
    }
    for(const auto& window:group.handoverWindows)
    {
        juce::ValueTree record("VirtualHandoverWindow");record.setProperty("leftNoteId",window.leftNoteId,nullptr);record.setProperty("rightNoteId",window.rightNoteId,nullptr);
        record.setProperty("fromOffsetSeconds",window.fromOffsetSeconds,nullptr);record.setProperty("toOffsetSeconds",window.toOffsetSeconds,nullptr);tree.addChild(record,-1,nullptr);
    }
    for(const auto& hop:group.hopPitchCurves)
    {
        NativeVirtualGroup payload;payload.id=nativeVirtualHopPitchId(group.id,hop.leftNoteId,hop.rightNoteId);payload.trackId=group.trackId;payload.orderedNoteIds={hop.leftNoteId,hop.rightNoteId};
        payload.pitchCurve=hop.pitchCurve;payload.pitchCurveEncoding=hop.pitchCurveEncoding;payload.pitchFromOffsetSeconds=hop.pitchFromOffsetSeconds;payload.pitchToOffsetSeconds=hop.pitchToOffsetSeconds;
        juce::ValueTree record("VirtualHopPitchCurve");record.addChild(nativeVirtualGroupTree(payload),-1,nullptr);tree.addChild(record,-1,nullptr);
    }
    return tree;
}
inline bool nativeVirtualGroupFromTree(const juce::ValueTree& tree,NativeVirtualGroup& output,juce::String& error)
{
    const auto fail=[&](const juce::String& why){error=why;return false;};
    if(!tree.hasType("NativeVirtualGroup")||!tree.getProperty("version").isInt()||int(tree.getProperty("version"))!=1)
        return fail("Unsupported virtual connection version");
    const auto keys=std::set<juce::String>{"version","id","trackId","enabled","automaticPitch","automaticallyCreated","pitchDurationSeconds","pitchFromOffsetSeconds","pitchToOffsetSeconds","sharedPitch","pitchCurveEncoding","coupledMovement"};
    for(int i=0;i<tree.getNumProperties();++i)if(!keys.count(tree.getPropertyName(i).toString()))return fail("Unknown virtual group property");
    NativeVirtualGroup group;
    if(!tree.getProperty("id").isString()||!tree.getProperty("trackId").isString())return fail("Invalid virtual group identity type");
    group.id=tree.getProperty("id").toString();group.trackId=tree.getProperty("trackId").toString();
    if(group.id.isEmpty()||group.trackId.isEmpty())return fail("Missing virtual group identity");
    for(const char* key:{"enabled","automaticPitch","automaticallyCreated"})if(!tree.getProperty(key).isBool())return fail("Invalid virtual group option type");
    group.enabled=bool(tree.getProperty("enabled"));group.automaticPitch=bool(tree.getProperty("automaticPitch"));group.automaticallyCreated=bool(tree.getProperty("automaticallyCreated"));
    if(tree.hasProperty("pitchCurveEncoding"))
    {if(!tree.getProperty("pitchCurveEncoding").isInt())return fail("Invalid virtual curve encoding type");group.pitchCurveEncoding=int(tree.getProperty("pitchCurveEncoding"));if(group.pitchCurveEncoding!=1&&group.pitchCurveEncoding!=2)return fail("Unsupported virtual curve encoding");}
    if(tree.hasProperty("coupledMovement"))
    {if(!tree.getProperty("coupledMovement").isBool())return fail("Invalid coupled movement type");group.coupledMovement=bool(tree.getProperty("coupledMovement"));}
    if(tree.hasProperty("sharedPitch"))
    {if(!tree.getProperty("sharedPitch").isBool())return fail("Invalid shared pitch mode");group.sharedPitch=bool(tree.getProperty("sharedPitch"));}
    if(tree.hasProperty("pitchDurationSeconds"))
    {
        const auto value=tree.getProperty("pitchDurationSeconds");
        if(!(value.isDouble()||value.isInt()||value.isInt64())||!std::isfinite(double(value))||double(value)<.002||double(value)>30)return fail("Invalid virtual pitch duration");
        group.pitchDurationSeconds=double(value);
    }
    std::set<juce::String> ids;
    for(const auto member:tree)
    {
        if(member.hasType("VirtualHandoverWindow"))
        {
            if(member.getNumProperties()!=4||member.getNumChildren()!=0
                ||!member.getProperty("leftNoteId").isString()||!member.getProperty("rightNoteId").isString())return fail("Invalid virtual handover window record");
            for(const char* key:{"fromOffsetSeconds","toOffsetSeconds"})
            {const auto value=member.getProperty(key);if(!(value.isDouble()||value.isInt()||value.isInt64())||!std::isfinite(double(value)))return fail("Invalid virtual handover offset type or value");}
            group.handoverWindows.push_back({member.getProperty("leftNoteId").toString(),member.getProperty("rightNoteId").toString(),
                double(member.getProperty("fromOffsetSeconds")),double(member.getProperty("toOffsetSeconds"))});continue;
        }
        if(member.hasType("VirtualHopPitchCurve"))
        {
            if(member.getNumProperties()!=0||member.getNumChildren()!=1)return fail("Invalid virtual hop curve record");
            const auto encoded=member.getChild(0);
            for(const auto child:encoded)if(!child.hasType("Member")&&!child.hasType("VirtualPitchPoint"))return fail("Nested hop payload cannot contain groups or source proofs");
            NativeVirtualGroup payload;if(!nativeVirtualGroupFromTree(encoded,payload,error))return false;
            if(payload.orderedNoteIds.size()!=2||payload.pitchCurve.empty()||!payload.enabled||payload.automaticPitch||payload.sharedPitch||!payload.coupledMovement||payload.automaticallyCreated||payload.trackId!=group.trackId
                ||payload.id!=nativeVirtualHopPitchId(group.id,payload.orderedNoteIds[0],payload.orderedNoteIds[1]))return fail("Invalid virtual hop pitch identity or policy");
            NativeVirtualHopPitchCurve hop;hop.leftNoteId=payload.orderedNoteIds[0];hop.rightNoteId=payload.orderedNoteIds[1];hop.pitchCurve=std::move(payload.pitchCurve);
            hop.pitchCurveEncoding=payload.pitchCurveEncoding;hop.pitchFromOffsetSeconds=payload.pitchFromOffsetSeconds;hop.pitchToOffsetSeconds=payload.pitchToOffsetSeconds;group.hopPitchCurves.push_back(std::move(hop));continue;
        }
        if(member.hasType("SplitTransitionEvidence"))
        {
            NativeSplitTransitionEvidence evidence;if(!nativeSplitTransitionEvidenceFromTree(member,evidence,error))return false;
            if(!group.splitTransition)group.splitTransition=std::move(evidence);
            else group.extraSplitTransitions.push_back(std::move(evidence));continue;
        }
        if(member.hasType("VirtualPitchPoint"))
        {
            if(member.getNumChildren()!=0||member.getNumProperties()!=8)return fail("Invalid virtual pitch point record");
            for(const char* key:{"u","residual","x1","y1","x2","y2"})
            {const auto value=member.getProperty(key);if(!(value.isDouble()||value.isInt()||value.isInt64()))return fail("Invalid virtual pitch number type");}
            if(!member.getProperty("shape").isInt()||!member.getProperty("precise").isBool())return fail("Invalid virtual pitch shape type");
            PitchCurveEditPoint p;p.timeSeconds=double(member.getProperty("u"));p.targetMidi=float(member.getProperty("residual"));p.shape=PitchCurveShape(int(member.getProperty("shape")));
            p.bezierX1=float(member.getProperty("x1"));p.bezierY1=float(member.getProperty("y1"));p.bezierX2=float(member.getProperty("x2"));p.bezierY2=float(member.getProperty("y2"));p.highPrecisionBezier=bool(member.getProperty("precise"));
            group.pitchCurve.push_back(p);continue;
        }
        if(!member.hasType("Member")||member.getNumProperties()!=1||member.getNumChildren()!=0||!member.getProperty("noteId").isString())return fail("Invalid virtual group member record");
        const auto id=member.getProperty("noteId").toString();if(id.isEmpty()||!ids.insert(id).second)return fail("Duplicate or empty virtual group member");
        group.orderedNoteIds.push_back(id);
    }
    if(group.splitTransition)
    {
        std::set<juce::String> halves,logical;
        const auto valid=[&](const NativeSplitTransitionEvidence& proof)
        {
            const auto at=std::find(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),proof.frontNoteId);
            return at!=group.orderedNoteIds.end()&&at!=group.orderedNoteIds.begin()&&std::distance(at,group.orderedNoteIds.end())>=3
                &&at[1]==proof.backNoteId&&halves.insert(proof.frontNoteId).second&&halves.insert(proof.backNoteId).second&&logical.insert(proof.logicalId).second;
        };
        if(!valid(*group.splitTransition))return fail("Split transition physical member order mismatch");
        for(const auto& proof:group.extraSplitTransitions)if(!valid(proof))return fail("Duplicate or overlapping logical split bridge proof");
    }
    else if(group.orderedNoteIds.size()<2)return fail("Invalid virtual group member count");
    std::set<std::pair<juce::String,juce::String>> hopKeys;
    for(const auto& hop:group.hopPitchCurves)
    {
        const auto left=std::find(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),hop.leftNoteId),right=std::find(group.orderedNoteIds.begin(),group.orderedNoteIds.end(),hop.rightNoteId);
        if(left==group.orderedNoteIds.end()||right==group.orderedNoteIds.end()||left>=right||!hopKeys.emplace(hop.leftNoteId,hop.rightNoteId).second)return fail("Duplicate, reversed or orphaned hop curve");
    }
    if(!group.pitchCurve.empty())
    {
        for(const char* key:{"pitchFromOffsetSeconds","pitchToOffsetSeconds"})
        {const auto value=tree.getProperty(key);if(!(value.isDouble()||value.isInt()||value.isInt64()))return fail("Missing virtual curve live endpoint offset");}
        group.pitchFromOffsetSeconds=double(tree.getProperty("pitchFromOffsetSeconds"));group.pitchToOffsetSeconds=double(tree.getProperty("pitchToOffsetSeconds"));
        if(!validateNativeVirtualPitchEncoding(group,error))return false;
    }
    if(!validateNativeVirtualHandoverWindows(group,error))return false;
    output=std::move(group);error.clear();return true;
}
}
