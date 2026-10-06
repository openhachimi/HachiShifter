#pragma once
#include "../backend/DiffSingerProjectCache.h"
#include "../backend/DiffSingerRenderer.h"
#include "../ProjectModel.h"
#include <iostream>
namespace hachi {
inline bool runDiffSingerCacheSmoke(const juce::File& output,const juce::File& bank,bool reopen) {
    using namespace backend;
    output.createDirectory();bool ok=true;juce::Array<juce::var> checks;
    const auto check=[&](const char* name,bool pass){ok=ok&&pass;std::cout<<name<<'='<<pass<<std::endl;
        auto* item=new juce::DynamicObject();item->setProperty("name",name);item->setProperty("passed",pass);checks.add(item);};
    const auto file=output.getChildFile("song.hjpx");
    if(!reopen) {ProjectModel p;juce::String error;check("project_saved",p.save(file,error));}
    DiffSingerRenderer::openProjectCache(reopen ? file : juce::File{});
    DiffSingerOptions options;options.backend=2;options.preview=1;DiffSingerRenderer::configure(options);
    UtauRenderRequest request;request.voicebankDirectory=bank;request.targetDurationSeconds=1.8;
    UtauNoteRenderSpec note;note.alias="ni";note.startSeconds=.4;note.durationSeconds=.8;note.midiNote=60;note.gain=1;request.notes.push_back(note);
    const auto json=DiffSingerRenderer::requestJson(request,"render");juce::AudioBuffer<float> audio;double rate=0;
    const auto started=juce::Time::getMillisecondCounterHiRes();const auto result=DiffSingerRenderer::invoke(json,{},&audio,&rate);
    const auto elapsed=juce::Time::getMillisecondCounterHiRes()-started;
    check("real_render_ok",(bool)result["ok"] && audio.getNumSamples()>0);
    if(!(bool)result["ok"]){std::cout<<result["error"].toString()<<std::endl;return false;}
    check("expected_disk_hit",(bool)result["disk_cache"]["hit"]==reopen);
    check("generation_information_present",result["parameters"].size()>0 && result["phonemes"].size()>0);
    juce::MemoryOutputStream samples; samples.writeDouble(rate);
    for(int ch=0;ch<audio.getNumChannels();++ch)samples.write(audio.getReadPointer(ch),(size_t)audio.getNumSamples()*sizeof(float));
    const auto hash=juce::SHA256(samples.getData(),samples.getDataSize()).toHexString();
    const auto signature=output.getChildFile("audio.sha256");
    if(reopen)check("fresh_process_audio_exact",signature.loadFileAsString()==hash);else signature.replaceWithText(hash);
    check("first_save_migrates_completed_cache",DiffSingerRenderer::saveProjectCache(file));
    const auto root=dsCache::directoryFor(file);check("sibling_cache_created",root.getParentDirectory()==output && dsCache::owned(root));
    auto full=json.clone();const auto base=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    full.getDynamicObject()->setProperty("vocoders",base.getChildFile("models/diffsinger/vocoders").getFullPathName());
    const auto engines=base.getChildFile("engines");const auto key=dsCache::keyFor(full,engines);
    check("stored_audio_and_metadata",root.getChildFile("entries/"+key+".wav").existsAsFile() && root.getChildFile("entries/"+key+".json").existsAsFile());
    check("captured_session_follows_save_as",dsCache::load(full,key,nullptr,nullptr).isObject());
    for(const auto& property: {juce::String("speaker"),juce::String("dictionary"),juce::String("language")}) {
        auto changed=full.clone();changed.getDynamicObject()->setProperty(juce::Identifier(property),"changed");
        check(("invalidate_"+property).toRawUTF8(),dsCache::keyFor(changed,engines)!=key);
    }
    for(const auto& property: {juce::String("lyric"),juce::String("pitch"),juce::String("parameters"),juce::String("expressions"),juce::String("timing")}) {
        auto changed=full.clone();changed["notes"][0].getDynamicObject()->setProperty(juce::Identifier(property),"changed");
        check(("invalidate_note_"+property).toRawUTF8(),dsCache::keyFor(changed,engines)!=key);
    }
    auto quality=full.clone();quality.getDynamicObject()->setProperty("inference",juce::JSON::parse(R"({"quality":"high"})"));
    check("invalidate_quality",dsCache::keyFor(quality,engines)!=key);
    auto reroll=full.clone();reroll.getDynamicObject()->setProperty("operation","parameters");
    check("explicit_regeneration_bypasses_cache",!dsCache::load(reroll,key,nullptr,nullptr).isObject());
    const auto dummy=output.getChildFile("model-fixture");dummy.createDirectory();const auto config=dummy.getChildFile("dsconfig.yaml");
    config.replaceWithText("one");auto fixture=full.clone();fixture.getDynamicObject()->setProperty("voicebank",dummy.getFullPathName());
    const auto firstKey=dsCache::keyFor(fixture,engines);config.replaceWithText("two");
    check("invalidate_changed_model_config",firstKey!=dsCache::keyFor(fixture,engines));
    DiffSingerRenderer::openProjectCache(output.getChildFile("other.hjpx"));
    auto other=full.clone();other.getDynamicObject()->setProperty("_cache_session",DiffSingerRenderer::cacheSession());
    check("project_caches_isolated",!dsCache::load(other,key,nullptr,nullptr).isObject());
    if(reopen) {
        // Corruption must become a cache miss; only generated fixture files are changed.
        const auto wav=root.getChildFile("entries/"+key+".wav");juce::MemoryBlock original;wav.loadFileAsData(original);
        wav.replaceWithText("broken");check("damaged_audio_rejected",!dsCache::load(full,key,nullptr,nullptr).isObject());wav.replaceWithData(original.getData(),original.getSize());
        const auto meta=root.getChildFile("entries/"+key+".json");const auto text=meta.loadFileAsString();meta.replaceWithText("{}");
        check("damaged_metadata_rejected",!dsCache::load(full,key,nullptr,nullptr).isObject());meta.replaceWithText(text);
        DiffSingerRenderer::openProjectCache(file);check("save_as_copies_cache",DiffSingerRenderer::saveProjectCache(output.getChildFile("renamed.hjpx")));
        auto renamed=full.clone();renamed.getDynamicObject()->setProperty("_cache_session",DiffSingerRenderer::cacheSession());
        check("save_as_cache_reusable",dsCache::load(renamed,key,nullptr,nullptr).isObject());
        check("save_as_preserves_original_cache",dsCache::owned(root));
    }
    auto* report=new juce::DynamicObject();report->setProperty("ok",ok);report->setProperty("checks",checks);report->setProperty("elapsed_ms",elapsed);
    output.getChildFile(reopen?"reopen.json":"first.json").replaceWithText(juce::JSON::toString(juce::var(report)));
    return ok;
}
}
