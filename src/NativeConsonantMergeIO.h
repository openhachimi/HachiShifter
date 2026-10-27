#pragma once
#include "ProjectModel.h"
#include <set>
namespace hachi
{
inline juce::ValueTree nativeConsonantMergeTree(const NativeConsonantMergeRecord& record)
{
    juce::ValueTree tree("ConsonantMerge");tree.setProperty("version",1,nullptr);
    for(const auto& [key,value]:std::initializer_list<std::pair<const char*,juce::String>>{
        {"id",record.id},{"trackId",record.trackId},{"mergedClipId",record.mergedClipId},{"mergedNoteId",record.mergedNoteId},
        {"onsetClipId",record.onsetClipId},{"onsetNoteId",record.onsetNoteId},{"sourceSha256",record.sourceSha256},
        {"originalStateBase64",record.originalStateBase64},{"mergedFingerprint",record.mergedFingerprint}})tree.setProperty(key,value,nullptr);
    tree.setProperty("onsetIndex",record.onsetIndex,nullptr);tree.setProperty("vowelIndex",record.vowelIndex,nullptr);return tree;
}
inline bool nativeConsonantMergeFromTree(const juce::ValueTree& tree,NativeConsonantMergeRecord& output,juce::String& error)
{
    const auto fail=[&](const juce::String& why){error=why;return false;};
    if(!tree.hasType("ConsonantMerge")||tree.getNumProperties()!=12||tree.getNumChildren()!=0||!tree.getProperty("version").isInt()||int(tree.getProperty("version"))!=1)return fail("Invalid consonant archive schema/version");
    for(const char* key:{"id","trackId","mergedClipId","mergedNoteId","onsetClipId","onsetNoteId","sourceSha256","originalStateBase64","mergedFingerprint"})
        if(!tree.getProperty(key).isString()||tree.getProperty(key).toString().isEmpty())return fail("Missing consonant archive identity/data");
    for(const char* key:{"onsetIndex","vowelIndex"})if(!tree.getProperty(key).isInt()||int(tree.getProperty(key))<0)return fail("Invalid consonant original placement index");
    NativeConsonantMergeRecord record;record.id=tree.getProperty("id").toString();record.trackId=tree.getProperty("trackId").toString();record.mergedClipId=tree.getProperty("mergedClipId").toString();record.mergedNoteId=tree.getProperty("mergedNoteId").toString();record.onsetClipId=tree.getProperty("onsetClipId").toString();record.onsetNoteId=tree.getProperty("onsetNoteId").toString();
    record.sourceSha256=tree.getProperty("sourceSha256").toString();record.originalStateBase64=tree.getProperty("originalStateBase64").toString();record.mergedFingerprint=tree.getProperty("mergedFingerprint").toString();record.onsetIndex=int(tree.getProperty("onsetIndex"));record.vowelIndex=int(tree.getProperty("vowelIndex"));
    if(record.sourceSha256.length()!=64||record.sourceSha256.retainCharacters("0123456789abcdefABCDEF").length()!=64||record.mergedFingerprint.length()!=64||record.mergedFingerprint.retainCharacters("0123456789abcdefABCDEF").length()!=64||record.originalStateBase64.length()>32*1024*1024||record.onsetClipId==record.mergedClipId||record.onsetNoteId==record.mergedNoteId)return fail("Invalid consonant archive bounds or source proof");
    juce::MemoryBlock bytes;if(!bytes.fromBase64Encoding(record.originalStateBase64)||bytes.getSize()==0)return fail("Corrupt original consonant fragment bytes");
    const auto backup=juce::ValueTree::readFromData(bytes.getData(),bytes.getSize());
    if(!backup.isValid()||backup.getChildWithName("Track").getNumChildren()<2)return fail("Consonant archive does not contain original source owners");
    output=std::move(record);error.clear();return true;
}
}
