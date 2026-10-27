#pragma once
#include "ProjectModel.h"
#include <cmath>
namespace hachi
{
inline juce::ValueTree nativeSplitTransitionEvidenceTree(const NativeSplitTransitionEvidence& e)
{
    juce::ValueTree tree("SplitTransitionEvidence");
    for(const auto& [key,value]:std::initializer_list<std::pair<const char*,juce::String>>{
        {"logicalId",e.logicalId},{"frontNoteId",e.frontNoteId},{"backNoteId",e.backNoteId},
        {"sourceObjectIdentity",e.sourceObjectIdentity},{"frontSourceObjectIdentity",e.frontSourceObjectIdentity},{"backSourceObjectIdentity",e.backSourceObjectIdentity},
        {"precedingElementIdentity",e.precedingElementIdentity},{"followingElementIdentity",e.followingElementIdentity},{"frontElementIdentity",e.frontElementIdentity},{"backElementIdentity",e.backElementIdentity}})tree.setProperty(key,value,nullptr);
    tree.setProperty("joinsPitches",e.joinsPitches,nullptr);tree.setProperty("joinsAmplitudes",e.joinsAmplitudes,nullptr);tree.setProperty("joinsPhases",e.joinsPhases,nullptr);tree.setProperty("transitionIdentified",e.transitionIdentified,nullptr);return tree;
}
inline bool nativeSplitTransitionEvidenceFromTree(const juce::ValueTree& tree,NativeSplitTransitionEvidence& output,juce::String& error)
{
    const auto fail=[&](const juce::String& why){error=why;return false;};
    if(!tree.hasType("SplitTransitionEvidence")||tree.getNumChildren()!=0||tree.getNumProperties()!=14)return fail("Invalid split transition evidence record");
    for(const char* key:{"logicalId","frontNoteId","backNoteId","sourceObjectIdentity","frontSourceObjectIdentity","backSourceObjectIdentity","precedingElementIdentity","followingElementIdentity","frontElementIdentity","backElementIdentity"})
        if(!tree.getProperty(key).isString()||tree.getProperty(key).toString().isEmpty())return fail("Missing split transition identity proof");
    for(const char* key:{"joinsPitches","joinsAmplitudes","joinsPhases","transitionIdentified"})if(!tree.getProperty(key).isBool()||!bool(tree.getProperty(key)))return fail("Missing split transition continuity proof");
    NativeSplitTransitionEvidence e;e.logicalId=tree.getProperty("logicalId").toString();e.frontNoteId=tree.getProperty("frontNoteId").toString();e.backNoteId=tree.getProperty("backNoteId").toString();
    e.sourceObjectIdentity=tree.getProperty("sourceObjectIdentity").toString();e.frontSourceObjectIdentity=tree.getProperty("frontSourceObjectIdentity").toString();e.backSourceObjectIdentity=tree.getProperty("backSourceObjectIdentity").toString();
    e.precedingElementIdentity=tree.getProperty("precedingElementIdentity").toString();e.followingElementIdentity=tree.getProperty("followingElementIdentity").toString();e.frontElementIdentity=tree.getProperty("frontElementIdentity").toString();e.backElementIdentity=tree.getProperty("backElementIdentity").toString();
    if(e.frontNoteId==e.backNoteId||e.sourceObjectIdentity!=e.frontSourceObjectIdentity||e.sourceObjectIdentity!=e.backSourceObjectIdentity||e.precedingElementIdentity!=e.frontElementIdentity||e.followingElementIdentity!=e.backElementIdentity)return fail("Inconsistent split transition graph proof");
    e.joinsPitches=e.joinsAmplitudes=e.joinsPhases=e.transitionIdentified=true;output=std::move(e);error.clear();return true;
}
inline juce::ValueTree nativeLogicalTransitionTree(const NativeLogicalTransition& annotation)
{
    juce::ValueTree tree("LogicalTransition");tree.setProperty("version",1,nullptr);
    tree.setProperty("sourceFrom",annotation.sourceFromSeconds,nullptr);tree.setProperty("sourceTo",annotation.sourceToSeconds,nullptr);
    tree.setProperty("middle",annotation.middleSourceSeconds,nullptr);tree.setProperty("confidence",annotation.confidence,nullptr);
    tree.addChild(nativeSplitTransitionEvidenceTree(annotation.evidence),-1,nullptr);return tree;
}
inline bool nativeLogicalTransitionFromTree(const juce::ValueTree& tree,NativeLogicalTransition& output,juce::String& error)
{
    const auto fail=[&](const juce::String& why){error=why;return false;};
    if(!tree.hasType("LogicalTransition")||tree.getNumProperties()!=5||tree.getNumChildren()!=1||!tree.getProperty("version").isInt()||int(tree.getProperty("version"))!=1)return fail("Invalid logical transition version or schema");
    NativeLogicalTransition annotation;
    if(!nativeSplitTransitionEvidenceFromTree(tree.getChild(0),annotation.evidence,error))return false;
    for(const char* key:{"sourceFrom","sourceTo","middle","confidence"}){const auto value=tree.getProperty(key);if(!(value.isDouble()||value.isInt()||value.isInt64())||!std::isfinite(double(value)))return fail("Invalid logical source annotation number");}
    annotation.sourceFromSeconds=double(tree.getProperty("sourceFrom"));annotation.sourceToSeconds=double(tree.getProperty("sourceTo"));annotation.middleSourceSeconds=double(tree.getProperty("middle"));annotation.confidence=float(tree.getProperty("confidence"));
    NativeTransitionShape shape{1,annotation.middleSourceSeconds,"identified-source-cut"};if(annotation.confidence<0||annotation.confidence>1||!validateNativeTransitionShape(shape,annotation.sourceFromSeconds,annotation.sourceToSeconds,error))return false;
    output=std::move(annotation);error.clear();return true;
}
}
