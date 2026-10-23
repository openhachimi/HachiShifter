#pragma once
#include "../AudioEngine.h"
#include "../backend/PlaybackRenderQueue.h"
#include <iostream>

namespace hachi
{
inline bool playbackRenderSmoke(const juce::File& folder, const juce::File& probeSource)
{
    using namespace backend;
    folder.createDirectory();
    juce::Array<juce::var> checks;
    bool ok = true;
    const auto check = [&](const juce::String& name, bool pass)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name",name);row->setProperty("ok",pass);
        checks.add(juce::var(row));ok &= pass;std::cout<<name<<"="<<pass<<std::endl;
    };
    const auto wait = [](auto predicate, int ms = 5000)
    {
        const auto end=juce::Time::getMillisecondCounterHiRes()+ms;
        while (!predicate() && juce::Time::getMillisecondCounterHiRes()<end) juce::Thread::sleep(2);
        return predicate();
    };
    struct Job final : juce::ThreadPoolJob
    {
        std::function<void(Job&)> body;
        explicit Job(std::function<void(Job&)> f):ThreadPoolJob("priority-check"),body(std::move(f)){}
        JobStatus runJob() override {body(*this);return jobHasFinished;}
    };
    const auto rank=playbackRenderRank;
    check("sounding_before_future",rank(9,12,10)<rank(11,12,10));
    check("future_before_past",rank(100,101,10)<rank(8,9,10));
    check("nearest_past_first",rank(8,9,10)<rank(0,1,10));
    check("half_open_boundary",rank(9,10,10).first==2 && rank(10,11,10).first==0);
    check("untimed_after_timed",rank(0,1,10)<rank(NAN,NAN,10));
    PlaybackNoteOrder notes({{100,102},{110,112},{120,122},{130,132}});
    check("note_absolute_timeline",notes.take(121)==2);
    check("note_seek_reorders_remaining",notes.take(100)==0 && notes.take(100)==1);
    check("note_no_duplicate",notes.take(100)==3 && !notes.take(100));
    {
        PlaybackRenderQueue queue(1);queue.beginUpdate();queue.setPosition(10);
        std::vector<int> order;std::atomic<int> done{0};std::atomic<int> discarded{0};
        const auto add=[&](int id,double from,double to, bool seek=false)
        {
            queue.add(std::make_unique<Job>([&,id,seek](Job&)
            {order.push_back(id);if(seek)queue.setPosition(80);done.fetch_add(1);}),
                {std::to_string(id),from,to,[&]{++discarded;}},RenderLane::diffSinger);
        };
        add(0,0,1);add(1,20,21);add(2,9,12,true);add(3,80,81);add(4,50,51);add(5,80,81);
        juce::Thread::sleep(20);check("batch_waits_for_all_tracks",done==0);
        queue.endUpdate({"0","1","2","3","4","5"});
        check("queued_seek_finishes",wait([&]{return done==6;}));queue.cancelAll();
        check("queued_seek_order_and_stable_ties",order==std::vector<int>({2,3,5,4,1,0}));
        check("seek_does_not_cancel_running_work",discarded==0);
        queue.beginUpdate();
        queue.add(std::make_unique<Job>([&](Job&){++done;}),{"obsolete",0,1,[&]{++discarded;}});
        queue.endUpdate({});check("obsolete_pending_removed",discarded==1 && done==6);
        queue.add(std::make_unique<Job>([&](Job&){++done;}));
        check("standalone_untimed_job_runs",wait([&]{return done==7;}));queue.cancelAll();
    }
    {
        PlaybackRenderQueue queue(2);std::atomic<bool> started{false},other{false},release{false},second{false};
        queue.add(std::make_unique<Job>([&](Job& job)
        {started=true;wait([&]{return release.load()||job.shouldExit();});}),{"held",0,1,{}},RenderLane::diffSinger);
        check("serial_lane_started",wait([&]{return started.load();}));
        queue.add(std::make_unique<Job>([&](Job&){second=true;}),{"same-lane",0,1,{}},RenderLane::diffSinger);
        queue.add(std::make_unique<Job>([&](Job&){other=true;}),{"other",0,1,{}},RenderLane::parallel);
        check("busy_ds_does_not_block_other_engines",wait([&]{return other.load();})&&!second);
        release=true;check("ds_request_kept_whole",wait([&]{return second.load();}));queue.cancelAll();
        started=false;std::atomic<bool> canceled{false};
        queue.add(std::make_unique<Job>([&](Job& job)
        {started=true;wait([&]{return job.shouldExit();});}),{{},0,1,[&]{canceled=true;}});
        check("cancel_started",wait([&]{return started.load();}));queue.cancelAll();
        check("cancel_drains_workers_and_releases_state",canceled.load());
        other=false;queue.add(std::make_unique<Job>([&](Job&){other=true;}));
        check("queue_reusable_after_cancel",wait([&]{return other.load();}));queue.cancelAll();
    }
    {
        PlaybackRenderQueue queue(2);
        std::atomic<bool> started{false}, retainedStarted{false}, release{false}, retainedCancelled{false};
        std::atomic<int> discarded{0}, published{0};
        queue.add(std::make_unique<Job>([&](Job& job)
        {
            started = true;
            wait([&] { return job.shouldExit() || release.load(); });
            if (!job.shouldExit()) ++published;
        }), {"old", 0, 2, [&] { ++discarded; }}, RenderLane::neural);
        queue.add(std::make_unique<Job>([&](Job& job)
        {
            retainedStarted = true;
            wait([&] { return release.load() || job.shouldExit(); });
            retainedCancelled = job.shouldExit();
        }), {"retained", 0, 2, {}}, RenderLane::parallel);
        check("revision_jobs_started", wait([&] { return started && retainedStarted; }));
        queue.beginUpdate();
        queue.add(std::make_unique<Job>([&](Job&) { ++published; }),
            {"new", 0, 2, {}}, RenderLane::neural);
        const auto since = juce::Time::getMillisecondCounterHiRes();
        queue.endUpdate({"new", "retained"});
        check("revision_cancel_does_not_wait_on_ui", juce::Time::getMillisecondCounterHiRes() - since < 100);
        check("obsolete_active_stops_and_replacement_runs", wait([&] { return discarded == 1 && published == 1; }, 1000));
        release = true;
        // Drain naturally before cancelAll, so the assertion really tests retention.
        check("unrelated_running_job_retained", wait([&] { return !queue.hasActiveJobs(); }) && !retainedCancelled);
        check("obsolete_result_not_published", published == 1 && discarded == 1);
        queue.cancelAll();
    }
    {
        RenderService service;service.beginUpdate();service.setPlaybackPosition(50);
        std::vector<int> order;std::atomic<int> done{0};
        for (int i=0;i<3;++i)
        {
            Mld5FileRenderRequest request;request.sourceFile=folder.getChildFile("missing.wav");
            request.pitchBackend=PitchRenderBackend::llsm2;
            service.renderMld5File(request,[&,i](auto){order.push_back(i);++done;},
                {std::to_string(i),double(i*50),double(i*50+1),{}});
        }
        service.endUpdate({"0","1","2"});check("real_service_completions",wait([&]{return done==3;}));
        service.cancelAll();check("real_service_uses_priority",order==std::vector<int>({1,2,0}));
    }
    const auto sample=folder.getChildFile("a.wav");
    {
        juce::AudioBuffer<float> wave(1,22050);
        for(int i=0;i<wave.getNumSamples();++i)wave.setSample(0,i,float(0.15*std::sin(i*440*juce::MathConstants<double>::twoPi/44100)));
        auto stream=sample.createOutputStream();juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),44100,1,16,{},0));
        check("fixture_written",writer&&writer->writeFromAudioSampleBuffer(wave,0,wave.getNumSamples()));
    }
    folder.getChildFile("oto.ini").replaceWithText("a.wav=a,0,30,-450,30,10\r\n");
    const auto probe=folder.getChildFile("priority-probe.exe");
    check("external_probe_available",probeSource.existsAsFile()&&probeSource.copyFileTo(probe));
    const auto log=juce::File(probe.getFullPathName()+".order");log.deleteFile();
    const auto release=juce::File(probe.getFullPathName()+".release");release.deleteFile();
    if(probe.existsAsFile())
    {
        UtauRenderRequest request;request.voicebankDirectory=folder;request.resamplerExecutable=probe;
        request.targetDurationSeconds=5;request.timelineStartSeconds=100;
        for(int i=0;i<4;++i){UtauNoteRenderSpec note;note.alias="a";note.startSeconds=1+i;note.durationSeconds=.25;note.midiNote=60+i;request.notes.push_back(note);}
        double position=103.1;request.priorityPosition=[&]{return position;};
        request.progress=[&](double value){if(value>.2)position=101.1;};
        const auto prioritized=UtauRenderer::render(request);
        const auto launches=juce::StringArray::fromLines(log.loadFileAsString());
        check("real_utau_first_note_at_playhead",launches.size()>=4&&launches[0].startsWith("D4|"));
        check("real_utau_seek_between_notes",launches.size()>=4&&launches[1].startsWith("C4|")&&launches[2].startsWith("C#4|")&&launches[3].startsWith("D#4|"));
        request.priorityPosition={};request.progress={};const auto chronological=UtauRenderer::render(request);
        double delta=0;bool same=prioritized.buffer.getNumSamples()>0&&prioritized.buffer.getNumSamples()==chronological.buffer.getNumSamples();
        if(same)for(int c=0;c<prioritized.buffer.getNumChannels();++c)for(int i=0;i<prioritized.buffer.getNumSamples();++i)
            delta=std::max(delta,std::abs(double(prioritized.buffer.getSample(c,i)-chronological.buffer.getSample(c,i))));
        check("priority_preserves_audio_and_crossfades",same&&delta==0);
        check("seek_reuses_note_cache",log.loadFileAsString()==launches.joinIntoString("\n")+"\n" || juce::StringArray::fromLines(log.loadFileAsString()).size()==launches.size());

        AudioEngine engine;engine.devices().closeAudioDevice();engine.setUtauResamplerFile(probe);
        ProjectData project;TrackData track;track.id="track";track.compose=true;track.pitchAlgorithm=PitchAlgorithm::utau;track.voicebankDirectory=folder;
        for(int i=0;i<2;++i){ClipData clip;clip.id="clip"+juce::String(i);clip.startSeconds=i*10;clip.durationSeconds=1;
            NoteData note;note.id="note"+juce::String(i);note.label="a";note.midiNote=70+i;note.startSeconds=.05;note.durationSeconds=.45;
            note.utauFlags=i==0?"g0WAIT":"g0FAST";clip.notes={note};track.clips.push_back(clip);}
        project.tracks={track};engine.setUtauRenderNoteSelection({"note0","note1"});engine.setPosition(10);
        engine.syncProject(project);
        check("transport_priority_connected",std::abs(engine.diagnosticRenderPriorityPosition()-10)<1e-6);
        check("near_region_becomes_ready_first",wait([&]{return !engine.playbackNeedsRender();}));
        check("far_region_still_rendering",engine.renderProgress().has_value());
        juce::String prematureError;
        check("export_rejects_unfinished_regions",!engine.exportWav(folder.getChildFile("premature.wav"),prematureError)&&prematureError.isNotEmpty());
        engine.prepareToPlay(128,48000);juce::AudioBuffer<float> out(2,128);engine.play();
        engine.getNextAudioBlock({&out,0,128});check("can_play_before_entire_project_done",engine.position()>10);
        engine.setPosition(0);check("seek_waits_for_new_region",engine.playbackNeedsRender());
        engine.getNextAudioBlock({&out,0,128});check("buffering_keeps_transport_and_mix_aligned",engine.position()==0&&out.getMagnitude(0,128)==0);
        release.replaceWithText("ready");check("all_regions_eventually_complete",wait([&]{return !engine.renderProgress();}));
        engine.getNextAudioBlock({&out,0,128});check("buffering_resumes_automatically",engine.position()>0);engine.stop();
        engine.setPosition(10);const auto before=engine.position();juce::String error;
        check("offline_export_waits_and_succeeds",engine.exportWav(folder.getChildFile("all.wav"),error));
        check("export_preserves_transport",std::abs(engine.position()-before)<1e-8);
        check("audition_open",engine.setAuditionFile(sample));engine.setPosition(.1);
        check("sample_audition_does_not_reprioritize_song",std::abs(engine.diagnosticRenderPriorityPosition()-10)<1e-8);
        engine.clearAuditionFile();check("audition_restores_song_priority",std::abs(engine.position()-10)<1e-8&&std::abs(engine.diagnosticRenderPriorityPosition()-10)<1e-8);
        ProjectModel fixture;auto savedProject=project;savedProject.tracks.front().clips.back().durationSeconds=20;
        for(auto& clip:savedProject.tracks.front().clips)for(auto& note:clip.notes)note.utauFlags="g0";
        fixture.replace(savedProject);check("regression_project_saved",fixture.save(folder.getChildFile("fixture.hjpx"),error));
        auto transportProject=savedProject;transportProject.tracks.front().compose=false;engine.syncProject(transportProject);
        engine.setPosition(10);engine.setPlayUntil(12);engine.play();
        for(int i=0;i<1000&&engine.isPlaying();++i)engine.getNextAudioBlock({&out,0,128});
        check("selection_stop_preserved",engine.position()>=12&&engine.position()<12.01&&!engine.isPlaying());
        engine.setPosition(10);engine.setPlayUntil(0);engine.play();
        for(int i=0;i<1000;++i)engine.getNextAudioBlock({&out,0,128});
        check("unlimited_playback_preserved",engine.position()>12.5);engine.stop();
    }
    auto* report=new juce::DynamicObject();report->setProperty("ok",ok);report->setProperty("checks",checks);
    folder.getChildFile("playback-priority-validation.json").replaceWithText(juce::JSON::toString(juce::var(report),true));
    return ok;
}
}
