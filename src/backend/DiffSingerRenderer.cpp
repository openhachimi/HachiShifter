#include "DiffSingerRenderer.h"
#include "DiffSingerProjectCache.h"
#include "AmplitudeEnvelopeCurve.h"
#include "DiffSingerTiming.h"
#include <chrono>
#include <mutex>
#if JUCE_WINDOWS
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace hachi::backend
{
namespace
{
std::timed_mutex inferenceMutex;
std::atomic<bool> runtimeClosing { false };
std::mutex optionsMutex;
DiffSingerOptions configuredOptions;
juce::String lastInferenceStatus;
juce::var lastInferenceReport;
juce::var failure(const juce::String& message)
{
    auto* data = new juce::DynamicObject();
    data->setProperty("ok", false);
    data->setProperty("error", message);
    return juce::var(data);
}
struct WorkDirectory
{
    juce::File file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("HachiShifter-DiffSinger-" + juce::Uuid().toString());
    ~WorkDirectory() { if (file.isDirectory()) file.deleteRecursively(); }
};

struct Worker
{
    juce::ChildProcess process;
    std::unique_ptr<WorkDirectory> directory;
    juce::String signature;
    ~Worker() { stop(); }
    void stop()
    {
        if (process.isRunning())
        {
            process.kill();
            process.waitForProcessToFinish(2000);
        }
        directory.reset();
        signature.clear();
    }
    bool ensure(const juce::File& engines)
    {
        auto next = engines.getFullPathName();
        for (const auto* name : { "bridge.py", "expressions.py", "compatibility.py", "runtime.py", "timing.py", "pronunciation.py", "english_g2p.py", "inference.py", "variance_retake.py" })
        {
            const auto file = engines.getChildFile("diffsinger").getChildFile(name);
            if (!file.existsAsFile()) return false;
            next += "|" + juce::String(file.getLastModificationTime().toMilliseconds())
                + ":" + juce::String(file.getSize());
        }
        if (process.isRunning() && signature == next) return true;
        stop();
        directory = std::make_unique<WorkDirectory>();
        if (directory->file.createDirectory().failed()) { stop(); return false; }
        #if JUCE_WINDOWS
        const auto parent = _getpid();
        #else
        const auto parent = getpid();
        #endif
        const juce::StringArray args { engines.getChildFile("python/python.exe").getFullPathName(),
            "-I", engines.getChildFile("diffsinger/bridge.py").getFullPathName(), "--worker",
            directory->file.getFullPathName(), juce::String(parent) };
        if (!process.start(args, 0)) { stop(); return false; }
        signature = next;
        return true;
    }
};

Worker& worker()
{
    static Worker instance;
    return instance;
}
}

void DiffSingerRenderer::openProjectCache(const juce::File& project) { dsCache::open(project); }
bool DiffSingerRenderer::saveProjectCache(const juce::File& project) { return dsCache::saveAs(project); }
juce::String DiffSingerRenderer::cacheSession() { return dsCache::session(); }

void DiffSingerRenderer::configure(const DiffSingerOptions& options)
{
    std::lock_guard lock(optionsMutex);
    configuredOptions = options;
}

juce::var DiffSingerRenderer::inferenceOptions(bool exporting)
{
    std::lock_guard lock(optionsMutex);
    return configuredOptions.request(exporting);
}

juce::String DiffSingerRenderer::inferenceStatus()
{
    std::lock_guard lock(optionsMutex);
    return lastInferenceStatus;
}

juce::var DiffSingerRenderer::inferenceReport()
{
    std::lock_guard lock(optionsMutex);
    return lastInferenceReport.clone();
}

void DiffSingerRenderer::shutdown()
{
    runtimeClosing.store(true);
    std::unique_lock guard(inferenceMutex);
    worker().stop();
    dsCache::cleanupTemporary();
}

bool DiffSingerRenderer::isVoicebank(const juce::File& directory)
{
    return directory.getChildFile("dsconfig.yaml").existsAsFile();
}

juce::var DiffSingerRenderer::invoke(juce::var request,
    const std::function<bool()>& cancelled, juce::AudioBuffer<float>* audio, double* rate)
{
    std::unique_lock guard(inferenceMutex, std::defer_lock);
    while (!guard.try_lock_for(std::chrono::milliseconds(50)))
        if (runtimeClosing.load() || (cancelled && cancelled())) return failure("DiffSinger cancelled");
    if (runtimeClosing.load() || (cancelled && cancelled())) return failure("DiffSinger cancelled");
    auto base = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    const auto runtimeOverride = juce::SystemStats::getEnvironmentVariable("HACHI_DIFFSINGER_RUNTIME", {});
    auto engines = runtimeOverride.isNotEmpty() ? juce::File(runtimeOverride) : base.getChildFile("engines");
    auto python = engines.getChildFile("python/python.exe");
    auto script = engines.getChildFile("diffsinger/bridge.py");
    if (!python.existsAsFile() || !script.existsAsFile())
        return failure(juce::String::fromUTF8("缺少 DiffSinger 运行库，请使用完整新版配布包（engines/python 与 engines/diffsinger）。"));
    if (request.getDynamicObject() == nullptr) return failure("Invalid DiffSinger request");
    request.getDynamicObject()->setProperty("vocoders", base.getChildFile("models/diffsinger/vocoders").getFullPathName());
    if(request["_cache_session"].toString().isEmpty())request.getDynamicObject()->setProperty("_cache_session",cacheSession());
    const auto cacheKey=dsCache::keyFor(request,engines);
    if(auto cached=dsCache::load(request,cacheKey,audio,rate);cached.isObject()) {
        if(cancelled && cancelled())return failure("DiffSinger cancelled");
        std::lock_guard lock(optionsMutex);lastInferenceReport=cached["inference"].clone();
        lastInferenceStatus=juce::String::fromUTF8("DiffSinger 工程缓存");return cached;
    }
    auto& current = worker();
    if (!current.ensure(engines)) return failure(juce::String::fromUTF8("无法启动 DiffSinger 后台，请检查 engines/diffsinger 运行库是否完整。"));
    const auto input = current.directory->file.getChildFile("request.json");
    const auto output = current.directory->file.getChildFile("result.json");
    const auto requestId = juce::Uuid().toString();
    request.getDynamicObject()->setProperty("request_id", requestId);
    if (!output.deleteFile() || !output.withFileExtension("wav").deleteFile())
    { current.stop(); return failure("Cannot clear previous DiffSinger result"); }
    const auto temporary = current.directory->file.getChildFile("request.pending");
    if (!temporary.replaceWithText(juce::JSON::toString(request)) || !temporary.moveFileTo(input))
    { current.stop(); return failure("Cannot write DiffSinger request"); }
    // Atomic files isolate requests without opening a network listener.
    const auto started = juce::Time::getMillisecondCounterHiRes();
    while (!output.existsAsFile())
    {
        if (runtimeClosing.load() || (cancelled && cancelled())
            || juce::Time::getMillisecondCounterHiRes() - started > 900'000.0)
        {
            current.stop();
            return failure(cancelled && cancelled() ? "DiffSinger cancelled"
                : "DiffSinger timed out after 15 minutes");
        }
        if (!current.process.isRunning())
        { current.stop(); return failure(juce::String::fromUTF8("DiffSinger 后台退出；请重试，后台会自动重新启动。")); }
        juce::Thread::sleep(20);
    }
    auto result = juce::JSON::parse(output);
    if (result.getDynamicObject() == nullptr || result["request_id"].toString() != requestId)
    { current.stop(); return failure("Invalid DiffSinger worker response"); }
    if (cancelled && cancelled()) { current.stop(); return failure("DiffSinger cancelled"); }
    if (result["inference"].isObject())
    {
        const auto state = result["inference"];
        juce::String status = state["backend"].toString();
        if (status.contains("DirectML")) status += " / GPU " + state["options"]["device"].toString();
        if (const auto* warnings = state["warnings"].getArray())
            for (const auto& warning : *warnings) status += "\n" + warning.toString();
        std::lock_guard lock(optionsMutex);
        lastInferenceStatus = status;
        lastInferenceReport = state;
    }
    if (!(bool) result["ok"]) return result;
    if (audio != nullptr)
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output.withFileExtension("wav")));
        if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0
            || reader->lengthInSamples / reader->sampleRate > 3600.1)
            return failure("DiffSinger returned invalid audio");
        audio->setSize(static_cast<int>(reader->numChannels), static_cast<int>(reader->lengthInSamples));
        reader->read(audio, 0, audio->getNumSamples(), 0, true, true);
        if (rate != nullptr) *rate = reader->sampleRate;
    }
    const auto stored=dsCache::store(request,cacheKey,result,output.withFileExtension("wav"));
    auto* cache=new juce::DynamicObject();cache->setProperty("hit",false);cache->setProperty("stored",stored);cache->setProperty("key",cacheKey);
    result.getDynamicObject()->setProperty("disk_cache",juce::var(cache));
    return result;
}

juce::var DiffSingerRenderer::requestJson(const UtauRenderRequest& source, const juce::String& operation)
{
    auto* data = new juce::DynamicObject();
    data->setProperty("operation", operation);
    data->setProperty("_cache_session", source.diffSingerCacheSession.isNotEmpty() ? source.diffSingerCacheSession : cacheSession());
    data->setProperty("voicebank", source.voicebankDirectory.getFullPathName());
    data->setProperty("language", source.diffSingerLanguage);
    data->setProperty("speaker", source.diffSingerSpeaker);
    data->setProperty("dictionary", source.diffSingerDictionary);
    data->setProperty("inference", source.diffSingerInference.isObject()
        ? source.diffSingerInference : inferenceOptions());
    data->setProperty("duration", source.targetDurationSeconds);
    juce::Array<juce::var> notes;
    for (std::size_t i = 0; i < source.notes.size(); ++i)
    {
        const auto& note = source.notes[i];
        auto* item = new juce::DynamicObject();
        item->setProperty("id", juce::String(static_cast<int>(i)));
        item->setProperty("start", note.startSeconds);
        item->setProperty("duration", note.durationSeconds);
        item->setProperty("midi", note.midiNote);
        item->setProperty("audible", note.gain != 0);
        item->setProperty("lyric", note.alias);
        item->setProperty("pronunciation", note.diffSingerPronunciation);
        item->setProperty("context", note.diffSingerContext);
        juce::Array<juce::var> points;
        for (const auto& point : note.pitchCurve)
            points.add(juce::var(juce::Array<juce::var>{ point.timeSeconds,
                note.midiNote + point.cents / 100.0f }));
        item->setProperty("pitch", points);
        if (note.diffSingerTiming.isNotEmpty())
            item->setProperty("timing", juce::JSON::parse(note.diffSingerTiming));
        auto* expressions = new juce::DynamicObject();
        auto* parameters = new juce::DynamicObject();
        for (const auto& [name, curve] : note.flagCurves)
            {
                if (!name.startsWith("DS:") || (!note.flagCurve && !name.startsWith("DS:ABS:"))) continue;
                juce::Array<juce::var> values;
                for (const auto& [time, value] : curve)
                    values.add(juce::var(juce::Array<juce::var>{time, value}));
                if (name.startsWith("DS:ABS:")) parameters->setProperty(name.substring(7), values);
                else expressions->setProperty(name, values);
            }
        item->setProperty("expressions", juce::var(expressions));
        item->setProperty("parameters", juce::var(parameters));
        notes.add(juce::var(item));
    }
    data->setProperty("notes", notes);
    return juce::var(data);
}

UtauRenderResult DiffSingerRenderer::render(const UtauRenderRequest& request)
{
    UtauRenderResult result;
    result.backend = "DiffSinger ONNX (CPU)";
    if (request.progress) request.progress(.05);
    const auto response = invoke(requestJson(request, "render"), request.cancelled, &result.buffer, &result.sampleRate);
    if (!(bool) response["ok"])
    {
        result.warning = response["error"].toString();
        result.buffer.setSize(0, 0);
        return result;
    }
    result.backend = response["backend"].toString();
    if((bool)response["disk_cache"]["hit"]) result.backend+=juce::String::fromUTF8(" · 工程缓存");
    if (request.noteParameters)
        if (const auto* rows=response["parameters"].getArray()) for (const auto& row:*rows) {
            const auto index=row["id"].toString().getIntValue();
            if (index>=0 && index<(int)request.notes.size()) request.noteParameters((std::size_t)index,row["curves"]);
        }
    std::vector<std::size_t> order(request.notes.size());
    for (std::size_t i=0; i<order.size(); ++i) order[i]=i;
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b)
    { return request.notes[a].startSeconds < request.notes[b].startSeconds; });
    std::vector<std::vector<UtauPhonemeSpan>> spans(request.notes.size());
    if (const auto* rows=response["phonemes"].getArray()) for (const auto& row:*rows)
    {
        if (row["id"].toString().isEmpty()) continue; // padding SP is not a note
        const auto index=row["id"].toString().getIntValue();
        if (index<0 || index>=(int)request.notes.size()) continue;
        auto owner=std::find(order.begin(),order.end(),(std::size_t)index);
        auto start=(double)row["start"]; const auto end=(double)row["end"];
        // A continued vowel belongs to each slur note during its own span.
        // Otherwise its entire waveform/gain would stay on the first note.
        while(owner!=order.end() && start<end)
        {
            auto next=std::next(owner);
            const auto continues=next!=order.end()
                && (request.notes[*next].alias.trim()=="+" || request.notes[*next].alias.trim()=="-");
            const auto stop=continues?std::min(end,request.notes[*next].startSeconds):end;
            const auto noteStart=request.notes[*owner].startSeconds;
            if(stop>start) spans[*owner].push_back({row["token"].toString(),row["kind"].toString(),
                start-noteStart,stop-noteStart});
            if(!continues || stop>=end) break;
            start=std::max(start,stop); owner=next;
        }
    }
    const auto onset=[&](std::size_t i)
    { return request.notes[i].startSeconds+(spans[i].empty()?0:spans[i].front().startSeconds); };
    const auto sampleAt=[&](double t)
    {return juce::jlimit(0,result.buffer.getNumSamples(),(int)std::llround(t*result.sampleRate));};
    for (std::size_t position=0; position<order.size(); ++position)
    {
        const auto i=order[position]; const auto& note=request.notes[i];
        const auto soundingStart=onset(i);
        const auto soundingEnd=spans[i].empty()?note.startSeconds+note.durationSeconds
            : note.startSeconds+spans[i].back().endSeconds;
        const auto envelope=diffSingerEnvelope(note.amplitudeEnvelope,
            soundingStart-note.startSeconds,soundingEnd-note.startSeconds);
        const auto gainAt=[envelope, gain=note.gain](double t)
        {
            float db=0;
            if (!envelope.empty())
            {
                auto right=std::lower_bound(envelope.begin(),envelope.end(),t,
                    [](const auto& point,double value){return point.timeSeconds<value;});
                if (right==envelope.begin()) db=right->gainDb;
                else if (right==envelope.end()) db=envelope.back().gainDb;
                else
                {
                    const auto& left=*(right-1);
                    const auto mix=juce::jlimit(0.0,1.0,(t-left.timeSeconds)/std::max(1e-9,right->timeSeconds-left.timeSeconds));
                    db=envelopeDbBetween(left.gainDb,right->gainDb,(float)mix,left.linearToNext);
                }
            }
            return gain*envelopeGainFromDb(db);
        };
        const auto start=sampleAt(soundingStart), end=sampleAt(soundingEnd);
        if (request.notePhonemes) request.notePhonemes(i,spans[i]);
        if (request.notePiece && end>start)
        {
            // Supply raw audio and its gains separately, just like WCSNDM.
            // Baking the envelope here made the roll apply it a second time.
            juce::AudioBuffer<float> piece(result.buffer.getNumChannels(),end-start);
            for (int ch=0;ch<piece.getNumChannels();++ch)
                piece.copyFrom(ch,0,result.buffer,ch,start,end-start);
            request.notePiece(i,piece,result.sampleRate,note.startSeconds-start/result.sampleRate,
                gainAt,[gain=note.gain](double){return gain;});
        }
        const auto gainStart=position==0?0:start;
        const auto gainEnd=position+1==order.size()?result.buffer.getNumSamples():sampleAt(onset(order[position+1]));
        for (int sample=gainStart;sample<gainEnd;++sample)
        {
            const auto gain=gainAt(sample/result.sampleRate-note.startSeconds);
            for (int ch=0;ch<result.buffer.getNumChannels();++ch)
                result.buffer.setSample(ch,sample,result.buffer.getSample(ch,sample)*gain);
        }
    }
    if (request.progress) request.progress(1);
    return result;
}
}
