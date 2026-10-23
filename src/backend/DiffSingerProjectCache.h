#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <mutex>

namespace hachi::backend::dsCache {
inline std::mutex mutex;
inline std::map<juce::String, juce::File> roots;
inline juce::String active;
constexpr int formatVersion = 1;
inline juce::File directoryFor(const juce::File& project) {
    return project.getSiblingFile(project.getFileName()+".ds-cache");
}
inline juce::String sessionLocked() {
    if (active.isEmpty()) {
        active=juce::Uuid().toString();
        roots[active]=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("HachiShifter-DS-Cache-"+active);
    }
    return active;
}
inline juce::String session() {std::lock_guard guard(mutex);return sessionLocked();}
inline void open(const juce::File& project) {
    std::lock_guard guard(mutex);active.clear();sessionLocked();
    if (project!=juce::File{}) roots[active]=directoryFor(project);
}
inline bool owned(const juce::File& root) {
    const auto info=juce::JSON::parse(root.getChildFile("cache-info.json"));
    return info["format"].toString()=="HachiShifter DiffSinger cache" && (int)info["version"]==formatVersion;
}
inline bool prepare(const juce::File& root) {
    if (owned(root)) return true;
    if (root.exists() && (!root.isDirectory() || root.getNumberOfChildFiles(juce::File::findFilesAndDirectories)>0)) return false;
    if(root.createDirectory().failed())return false;
    auto* info=new juce::DynamicObject();info->setProperty("format","HachiShifter DiffSinger cache");
    info->setProperty("version",formatVersion);info->setProperty("editor_version",JUCE_APPLICATION_VERSION_STRING);
    return root.getChildFile("cache-info.json").replaceWithText(juce::JSON::toString(juce::var(info)));
}
inline void removeTemporary(const juce::File& root,const juce::String& token) {
    // Only this session's generated child of the OS temporary directory is removable.
    const auto temp=juce::File::getSpecialLocation(juce::File::tempDirectory);
    if(root.getParentDirectory()==temp && root.getFileName()=="HachiShifter-DS-Cache-"+token && owned(root))
        root.deleteRecursively();
}
inline void cleanupTemporary() {
    std::lock_guard guard(mutex);for(const auto& [token,root]:roots)removeTemporary(root,token);
}
inline bool saveAs(const juce::File& project) {
    std::lock_guard guard(mutex);const auto token=sessionLocked();const auto previous=roots[token];
    const auto next=directoryFor(project);if(previous==next)return true;
    roots[token]=next;
    // Copy only our generated entries. Never rename/delete a user's project or old cache.
    if (owned(previous)) {
        if (!prepare(next)) return false;
        const auto dest=next.getChildFile("entries");if(dest.createDirectory().failed())return false;
        for(const auto& file:previous.getChildFile("entries").findChildFiles(juce::File::findFiles,false,"*")) {
            if (!file.hasFileExtension("json;wav") || file.getFileNameWithoutExtension().length()!=64)continue;
            const auto target=dest.getChildFile(file.getFileName());
            if(!file.copyFileTo(target))return false;
        }
    }
    removeTemporary(previous,token);
    roots[token]=next;return true;
}
inline juce::String fileHash(const juce::File& file) {return juce::SHA256(file).toHexString();}
inline juce::String keyFor(const juce::var& request,const juce::File& engines) {
    auto canonical=request.clone();auto* object=canonical.getDynamicObject();if(!object)return {};
    for(const auto* key:{"_cache_session","request_id"})object->removeProperty(key);
    juce::MemoryOutputStream bytes;bytes.writeInt(formatVersion);bytes.writeString("DS renderer 025");
    bytes.writeString(juce::JSON::toString(canonical,true));
    juce::StringArray paths;
    for(const auto& root:{juce::File(request["voicebank"].toString()),juce::File(request["vocoders"].toString())}) {
        paths.add(root.getFullPathName());
        for(const auto& file:root.findChildFiles(juce::File::findFiles,true,"*"))
            if(file.hasFileExtension("onnx;yaml;yml;json;txt;emb;bin;data;npz;npy;csv;dic"))paths.add(file.getFullPathName());
    }
    // Fingerprint application scripts and pronunciation resources, not thousands
    // of vendored package source files. Package versions are recorded by METADATA.
    const auto runtime=engines.getChildFile("diffsinger");paths.add(runtime.getFullPathName());
    for(const auto& file:runtime.findChildFiles(juce::File::findFiles,false,"*.py"))paths.add(file.getFullPathName());
    for(const auto* folder:{"pronunciation-data","pypinyin"})
        for(const auto& file:runtime.getChildFile(folder).findChildFiles(juce::File::findFiles,true,"*"))
            if(file.hasFileExtension("py;json;txt;dict;npz;npy;csv;dic"))paths.add(file.getFullPathName());
    for(const auto& folder:runtime.getChildFile("packages").findChildFiles(juce::File::findDirectories,false,"*.dist-info"))
        paths.add(folder.getChildFile("METADATA").getFullPathName());
    const auto dictionary=request["dictionary"].toString();if(juce::File::isAbsolutePath(dictionary))paths.add(dictionary);
    paths.sort(false);paths.removeDuplicates(false);
    for(const auto& path:paths) {
        const juce::File file(path);bytes.writeString(path);
        if (file.isDirectory()) { bytes.writeString("directory"); continue; }
        bytes.writeInt64(file.getSize());
        bytes.writeInt64(file.getLastModificationTime().toMilliseconds());
        if(file.existsAsFile() && file.getSize()<1024*1024)bytes.writeString(fileHash(file));
    }
    return juce::SHA256(bytes.getData(),bytes.getDataSize()).toHexString();
}
inline juce::File rootFor(const juce::var& request) {
    const auto found=roots.find(request["_cache_session"].toString());return found==roots.end()?juce::File{}:found->second;
}
inline bool readAudio(const juce::File& file,juce::AudioBuffer<float>* audio,double* rate) {
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader || reader->sampleRate<=0 || reader->lengthInSamples<=0 || reader->numChannels<1 || reader->numChannels>2
        || reader->lengthInSamples/reader->sampleRate>3600.1)return false;
    if(audio) {
        audio->setSize((int)reader->numChannels,(int)reader->lengthInSamples);
        if(!reader->read(audio,0,audio->getNumSamples(),0,true,true))return false;
    }
    if(rate)*rate=reader->sampleRate;return true;
}
inline juce::var load(const juce::var& request,const juce::String& key,juce::AudioBuffer<float>* audio,double* rate) {
    const auto operation=request["operation"].toString();
    if(operation!="render" && operation!="timing" && operation!="phonemize")return {};
    std::lock_guard guard(mutex);const auto root=rootFor(request);if(root==juce::File{} || !owned(root))return {};
    const auto stem=root.getChildFile("entries").getChildFile(key);
    auto envelope=juce::JSON::parse(stem.withFileExtension("json"));
    auto result=envelope["result"].clone();
    if(envelope["key"].toString()!=key || !(bool)result["ok"] || !result.isObject()
        || envelope["result_hash"].toString()!=juce::SHA256(juce::JSON::toString(result,true).toUTF8()).toHexString())return {};
    if(operation=="render") {
        const auto wav=stem.withFileExtension("wav");
        if(!wav.existsAsFile() || envelope["audio_hash"].toString()!=fileHash(wav) || !readAudio(wav,audio,rate))return {};
    }
    auto* cache=new juce::DynamicObject();cache->setProperty("hit",true);cache->setProperty("key",key);cache->setProperty("directory",root.getFullPathName());
    result.getDynamicObject()->setProperty("disk_cache",juce::var(cache));
    return result;
}
inline bool store(const juce::var& request,const juce::String& key,const juce::var& result,const juce::File& wav) {
    if(!(bool)result["ok"] || request["operation"].toString()=="inspect")return false;
    std::lock_guard guard(mutex);const auto root=rootFor(request);if(root==juce::File{} || !prepare(root))return false;
    const auto entries=root.getChildFile("entries");if(entries.createDirectory().failed())return false;
    const auto stem=entries.getChildFile(key);const auto nonce=juce::Uuid().toString();
    auto clean=result.clone();clean.getDynamicObject()->removeProperty("request_id");clean.getDynamicObject()->removeProperty("disk_cache");
    auto* envelope=new juce::DynamicObject();juce::var record(envelope);
    envelope->setProperty("key",key);envelope->setProperty("operation",request["operation"]);
    envelope->setProperty("created",juce::Time::getCurrentTime().toISO8601(true));
    envelope->setProperty("voicebank",request["voicebank"]);envelope->setProperty("inference",request["inference"]);
    envelope->setProperty("result",clean);
    envelope->setProperty("result_hash",juce::SHA256(juce::JSON::toString(clean,true).toUTF8()).toHexString());
    if(request["operation"].toString()=="render") {
        if(!readAudio(wav,nullptr,nullptr))return false;
        const auto pending=entries.getChildFile(nonce+".pending.wav");
        if(!wav.copyFileTo(pending))return false;
        envelope->setProperty("audio_hash",fileHash(pending));
        if(!pending.moveFileTo(stem.withFileExtension("wav"))){pending.deleteFile();return false;}
    }
    const auto pending=entries.getChildFile(nonce+".pending.json");
    if(!pending.replaceWithText(juce::JSON::toString(record)) || !pending.moveFileTo(stem.withFileExtension("json"))) {
        pending.deleteFile();return false;
    }
    return true;
}
}
