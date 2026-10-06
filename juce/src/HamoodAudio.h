#pragma once
#include "Hamood.h"
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>

namespace hachi::hamoodaudio
{
inline juce::var object() { return new juce::DynamicObject(); }
inline void set(juce::var& v,const char* k,juce::var x) {v.getDynamicObject()->setProperty(k,std::move(x));}
inline juce::var failure(const juce::String& e) {auto v=object();set(v,"ok",false);set(v,"error",e);return v;}
inline juce::File cacheFolder(const juce::File& project)
{
    return project==juce::File() ? juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("HachiShifter-HAMOOD-cache")
        : project.getSiblingFile(project.getFileName()+".hamood-cache");
}
inline const ClipData* audioClip(const ProjectData& data,const juce::String& id)
{
    for(const auto& t:data.tracks)for(const auto& c:t.clips)
        if(c.id==id && c.parts.empty() && (t.accompaniment || (!t.compose && std::abs(c.durationSeconds-c.sourceDurationSeconds)<1.e-5)) && c.sourceFile.existsAsFile())return &c;
    return nullptr;
}
inline juce::File indexFile(const juce::File& folder,const ClipData& clip)
{return folder.getChildFile(juce::SHA256(clip.sourceFile.getFullPathName().toUTF8()).toHexString()+".index.json");}
inline juce::var cached(const juce::File& folder,const ClipData& clip)
{
    const auto index=juce::JSON::parse(indexFile(folder,clip));
    const auto filename=index["file"].toString();
    if(filename.isEmpty() || filename.containsChar('/') || filename.containsChar('\\'))return failure("Run hamood_analyse_audio first");
    auto result=juce::JSON::parse(folder.getChildFile(filename));
    if(!(bool)result["ok"] || !result["chords"].isArray() || !result["beats"].isArray() || !result["downbeats"].isArray() || result["schema"].toString()!="036-audio-v2"
        || result["identity"]["decoder"].toString()!="juce-reader"
        || (juce::int64)result["source_size"]!=clip.sourceFile.getSize()
        || (juce::int64)result["source_mtime_ms"]!=clip.sourceFile.getLastModificationTime().toMilliseconds())
        return failure("Audio/cache changed; run hamood_analyse_audio again");
    for(const auto& row:*result["chords"].getArray())
        if(!row.isObject() || !row["pitch_classes"].isArray() || !std::isfinite((double)row["start"])
            || !std::isfinite((double)row["end"]) || (double)row["end"]<=(double)row["start"])
            return failure("Invalid chord cache; run hamood_analyse_audio again");
    set(result,"cache_hit",true);return result;
}
inline juce::var analyse(const ProjectData& data,const juce::String& clipId,const juce::File& folder,bool force,const std::shared_ptr<std::atomic<bool>>& cancel)
{
    const auto* clip=audioClip(data,clipId);if(!clip)return failure("Choose an existing single-source accompaniment/original-audio clip; split merged regions before analysing them");
    if(!force){auto hit=cached(folder,*clip);if((bool)hit["ok"])return hit;}
    auto config=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getSiblingFile("engines").getChildFile("hamood/runtime.json");
    const auto configured=juce::SystemStats::getEnvironmentVariable("HACHI_HAMOOD_RUNTIME",{});
    if(configured.isNotEmpty())config=juce::File(configured);
    const auto runtime=juce::JSON::parse(config);
    const auto pythonPath=runtime["python"].toString();
    const auto python=pythonPath.isNotEmpty() ? config.getParentDirectory().getChildFile(pythonPath) : juce::File();
    const auto script=config.getSiblingFile("worker.py");
    if(!python.existsAsFile()||!script.existsAsFile())
        return failure("HAMOOD audio runtime not installed: configure engines/hamood/runtime.json with a Python interpreter containing torch, torchaudio, librosa, soundfile, scipy, soxr and yaml");
    if(folder.createDirectory().failed())return failure("Cannot create HAMOOD analysis cache");
    const auto key=juce::Uuid().toString();auto input=folder.getChildFile(key+".request.json"),output=folder.getChildFile(key+".result.json");
    struct Decoded { juce::File file; ~Decoded(){file.deleteFile();} } decoded{folder.getChildFile(key+".decoded.wav")};
    // Use the editor's decoder, including its MP3 priming/padding. External
    // MP3 decoders can disagree by tens of milliseconds on identical bytes.
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(clip->sourceFile));
    if(!reader)return failure("The editor cannot decode this audio source");
    {
        auto stream=decoded.file.createOutputStream();juce::WavAudioFormat wav;
        if(!stream)return failure("Cannot write decoded analysis audio");
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),reader->sampleRate,reader->numChannels,32,{},0));
        if(!writer || !writer->writeFromAudioReader(*reader,0,reader->lengthInSamples))return failure("Could not decode analysis audio");
    }
    auto request=object();set(request,"audio_path",clip->sourceFile.getFullPathName());set(request,"cache_dir",folder.getFullPathName());set(request,"force",force);
    set(request,"decoded_audio_path",decoded.file.getFullPathName());set(request,"decoder","juce-reader");
    if(!input.replaceWithText(juce::JSON::toString(request,true)))return failure("Cannot write analysis request");
    juce::ChildProcess process;
    if(!process.start(juce::StringArray{python.getFullPathName(),"-I","-B",script.getFullPathName(),"--request",input.getFullPathName(),"--output",output.getFullPathName()},0))
    {input.deleteFile();return failure("Could not start HAMOOD audio worker");}
    const auto started=juce::Time::getMillisecondCounterHiRes();
    while(process.isRunning())
    {
        if(cancel->load() || juce::Time::getMillisecondCounterHiRes()-started>1800000)
        {process.kill();process.waitForProcessToFinish(2000);input.deleteFile();output.deleteFile();return failure("Analysis cancelled or timed out");}
        juce::Thread::sleep(40);
    }
    auto result=juce::JSON::parse(output);input.deleteFile();output.deleteFile();
    if(!(bool)result["ok"])return result.isObject()?result:failure("HAMOOD audio worker failed without a result");
    const juce::File cache(result["cache_file"].toString());
    if(cache.getParentDirectory()!=folder || !cache.existsAsFile())return failure("Invalid analysis cache result");
    auto index=object();set(index,"file",cache.getFileName());indexFile(folder,*clip).replaceWithText(juce::JSON::toString(index,true));return result;
}
inline double projectTime(const ClipData& c,double sourceTime)
{return c.startSeconds+sourceTime-c.sourceOffsetSeconds;}
inline juce::var summary(const juce::var& result,const ClipData& c)
{
    if(!(bool)result["ok"])return result;
    auto value=object();set(value,"ok",true);set(value,"clip_id",c.id);set(value,"analysis_id",result["identity"]["audio_sha256"]);
    set(value,"models",result["models"]);set(value,"bpm",result["bpm"]);set(value,"cache_hit",result["cache_hit"]);
    set(value,"processing_seconds",result["processing_seconds"]);set(value,"chord_count",result["chords"].size());set(value,"beat_count",result["beats"].size());
    set(value,"tempo_note","BPM is a rough median of quantized beat intervals; half/double tempo and missing beats are possible. Do not overwrite the project tempo automatically.");
    set(value,"source_duration",result["duration"]);set(value,"clip_start_seconds",c.startSeconds);
    set(value,"score_semantics",result["score_semantics"]);set(value,"next_tool","hamood_audio_context");return value;
}
inline juce::var context(const ProjectData& data,const ClipData& c,const juce::var& result,double from,double to,int offset,int limit)
{
    if(!(bool)result["ok"])return result;
    auto value=summary(result,c);juce::Array<juce::var> chords,beats,downbeats;
    int total=0;
    for(auto row:*result["chords"].getArray())
    {
        const auto start=std::max(c.startSeconds,projectTime(c,(double)row["start"]));
        const auto end=std::min(c.startSeconds+c.durationSeconds,projectTime(c,(double)row["end"]));
        if(end<=start || end<=from || start>=to)continue;
        if(total++<offset || chords.size()>=limit)continue;
        set(row,"source_start",row["start"]);set(row,"source_end",row["end"]);set(row,"start",start);set(row,"end",end);
        const double q=data.quarterPositionForSeconds(start),bar=data.numerator*4.0/std::max(1,data.denominator);
        set(row,"bar",(int)std::floor(q/bar)+1);set(row,"quarter_position",q);chords.add(row);
    }
    // Beat events use their own compact range; hard cap prevents unbounded dumps.
    for(const auto* field:{"beats","downbeats"})
        if(auto* rows=result[field].getArray())for(const auto& t:*rows)
        {
            const auto time=projectTime(c,(double)t);
            if(time<from||time>=to||time<c.startSeconds||time>=c.startSeconds+c.durationSeconds)continue;
            auto& out=juce::String(field)=="beats"?beats:downbeats;if(out.size()<4096)out.add(time);
        }
    set(value,"chords",chords);set(value,"beats",beats);set(value,"downbeats",downbeats);set(value,"total",total);
    set(value,"next_offset",offset+chords.size()<total?juce::var(offset+chords.size()):juce::var());
    set(value,"from_seconds",from);set(value,"to_seconds",to);return value;
}
inline double median(std::vector<double> values)
{if(values.empty())return 0;std::sort(values.begin(),values.end());const auto n=values.size();return n%2?values[n/2]:(values[n/2-1]+values[n/2])*.5;}
inline juce::var alignment(const ProjectData& data,const ClipData& c,const juce::var& evidence,double from,double to,int division)
{
    if(!(bool)evidence["ok"])return evidence;
    std::vector<double> errors,times;
    for(const auto& beat:*evidence["beats"].getArray())
    {
        const auto t=projectTime(c,(double)beat);if(t<from||t>=to||t<c.startSeconds||t>=c.startSeconds+c.durationSeconds)continue;
        const auto q=data.quarterPositionForSeconds(t),nearest=std::round(q*division)/division;
        errors.push_back(t-data.secondsForQuarterPosition(nearest));times.push_back(t);
    }
    if(errors.size()<8)return failure("At least 8 detected beats are needed for phase alignment");
    const auto shift=-median(errors);std::vector<double> before,after;
    for(auto e:errors){before.push_back(std::abs(e));after.push_back(std::abs(e+shift));}
    auto out=object();set(out,"ok",true);set(out,"clip_id",c.id);set(out,"beat_count",(int)errors.size());
    set(out,"project_bpm",data.bpm);set(out,"grid_divisions_per_quarter",division);set(out,"current_start_seconds",c.startSeconds);
    set(out,"suggested_shift_seconds",shift);set(out,"suggested_start_seconds",c.startSeconds+shift);
    set(out,"median_error_before_seconds",median(before));set(out,"median_error_after_seconds",median(after));
    set(out,"method","Local beat phase against the current project grid; no audio or tempo change");
    set(out,"limitation","20 ms beat resolution; whole-beat/bar offsets remain ambiguous. Preview and verify before move_clip. A shift does not correct tempo drift.");
    juce::Array<juce::var> windows;
    for(int i=0;i<3;++i)
    {
        const auto lo=from+(to-from)*i/3,hi=from+(to-from)*(i+1)/3;
        std::vector<double> local;for(size_t k=0;k<times.size();++k)if(times[k]>=lo&&times[k]<hi)local.push_back(errors[k]);
        auto row=object();set(row,"from_seconds",lo);set(row,"to_seconds",hi);set(row,"beat_count",(int)local.size());
        set(row,"median_error_seconds",local.empty()?juce::var():juce::var(median(local)));windows.add(row);
    }
    set(out,"windows",windows);return out;
}
inline bool attach(const ProjectData& data,const juce::String& clipId,const juce::File& folder,hamood::Options& options,juce::String& error)
{
    const auto* clip=audioClip(data,clipId);if(!clip){error="Unknown accompaniment/original-audio clip";return false;}
    const auto evidence=cached(folder,*clip);if(!(bool)evidence["ok"]){error=evidence["error"].toString();return false;}
    for(const auto& row:*evidence["chords"].getArray())
    {
        hamood::Chord c;c.start=std::max(clip->startSeconds,projectTime(*clip,(double)row["start"]));
        c.end=std::min(clip->startSeconds+clip->durationSeconds,projectTime(*clip,(double)row["end"]));
        if(c.end<=c.start)continue;c.label=row["label"].toString();c.score=(double)row["score"];
        if(auto* pitches=row["pitch_classes"].getArray())for(const auto& p:*pitches)c.pitches.push_back((int)p);
        options.chords.push_back(c);
    }
    options.audioClipId=clipId;return true;
}
}
