#pragma once
#include "../Hamood.h"
#include "../AudioEngine.h"
#include "../backend/McpServer.h"
#include <iostream>

namespace hachi
{
inline bool runHamoodUtauSmoke(const juce::File& directory,const juce::File& sourceWav,const juce::File& resampler)
{
    directory.createDirectory();bool ok=true;int count=0;juce::Array<juce::var> checks;
    auto check=[&](const juce::String& name,bool value){++count;ok&=value;std::cout<<(value?"PASS ":"FAIL ")<<name<<std::endl;auto* row=new juce::DynamicObject();row->setProperty("name",name);row->setProperty("ok",value);checks.add(row);};
    const auto bank=directory.getChildFile("voicebank");bank.createDirectory();
    check("real source sample copied into isolated bank",sourceWav.copyFileTo(bank.getChildFile("a.wav")));
    bank.getChildFile("oto.ini").replaceWithText("a.wav=a,41,87.688,97.316,8.938,4.457\n");
    auto sameFlags=[](const NoteData& a,const NoteData& b){
        const auto left=backend::McpServer::noteJson(a),right=backend::McpServer::noteJson(b);
        for(const auto* key:{"utau_flags","flag_split","region_flags","flag_curve_enabled","flag_curves"})
            if(juce::JSON::toString(left[key])!=juce::JSON::toString(right[key]))return false;
        if(a.utauFlagCurves.size()!=b.utauFlagCurves.size())return false;
        for(size_t i=0;i<a.utauFlagCurves.size();++i)
        {
            const auto& ac=a.utauFlagCurves[i];const auto& bc=b.utauFlagCurves[i];
            if(ac.flag!=bc.flag||ac.points.size()!=bc.points.size())return false;
            for(size_t j=0;j<ac.points.size();++j)
            {
                const auto& x=ac.points[j];const auto& y=bc.points[j];
                if(x.timeSeconds!=y.timeSeconds||x.value!=y.value||x.shape!=y.shape||x.bezierX1!=y.bezierX1||x.bezierY1!=y.bezierY1||x.bezierX2!=y.bezierX2||x.bezierY2!=y.bezierY2)return false;
            }
        }
        return true;
    };
    for(const auto mode:{UtauMode::classic,UtauMode::jie,UtauMode::mou})
    {
        const auto modeName=utauModeKey(mode);
        ProjectData base;TrackData track;track.id="lead";track.name=modeName;track.compose=true;
        track.pitchAlgorithm=PitchAlgorithm::utau;track.utauMode=mode;track.voicebankDirectory=bank;
        track.utauGlobalFlags="g-12Mb20K1Mj8Xcustom17";track.utauConsonantVelocity=125;
        ClipData clip;clip.id="clip";clip.startSeconds=1;clip.durationSeconds=3;
        NoteData note;note.id="a-note";note.label="a";note.midiNote=60;note.startSeconds=.2;note.durationSeconds=1.0;
        note.utauFlags="g5Mb80B30K2Mj25Unknown-7";
        note.utauFlagSplit=true;note.utauRegionFlags1="g-5Mb0";note.utauRegionFlags2="";note.utauRegionFlags3="g25Mb90";note.utauRegionFlags4="Mb10";
        note.utauFlagCurveEnabled=true;
        for(const auto& kind:flagCurveKinds())
        {
            FlagCurve curve;curve.flag=kind.flag;
            curve.points={{-.06,kind.minimum,PitchCurveShape::linear},{.35,kind.defaultValue,PitchCurveShape::customBezier},{1.1,kind.maximum,PitchCurveShape::smooth}};
            curve.points[1].bezierX1=.2f;curve.points[1].bezierY1=.4f;curve.points[1].bezierX2=.7f;curve.points[1].bezierY2=.9f;
            note.utauFlagCurves.push_back(curve);
        }
        note.utauFlagCurves.push_back({"CustomFlag",{{0,4},{1,-8}}});
        // Non-DS tracks may retain dormant DS data after switching voicebanks.
        note.utauFlagCurves.push_back({"DS:AUTO:BREC",{{0,-20}}});note.diffSingerTiming="preserved dormant DS data";
        note.utauConsonantVelocity=133;note.utauPreutteranceOverrideEnabled=true;note.utauPreutteranceSeconds=.04;
        note.utauOverlapOverrideEnabled=true;note.utauOverlapSeconds=.01;note.utauStpSeconds=.005;
        note.utauJieSplitSet=true;note.utauJieSplit1=.1;note.utauJieSplit2=.3;note.utauJieSplit3=.9;
        note.utauOto={true,41,87.688,97.316,8.938,4.457,true,30,90,210,"CVVC"};note.utauSplice=true;
        clip.notes.push_back(note);
        for(const auto* alias:{"R","AP","SP","RR"}){auto extra=note;extra.id=alias;extra.label=alias;extra.startSeconds=1.4;clip.notes.push_back(extra);}
        track.clips.push_back(clip);base.tracks.push_back(track);
        hamood::Options options;options.trackId="lead";options.noteIds={"a-note"};options.keyMode="manual";options.voices={2};
        auto all=options;all.wholeTrack=true;const auto allPlan=hamood::analyse(base,all);
        check(modeName+" keeps R/AP/SP aliases and skips editor RR rest",allPlan.notes.size()==4);
        for(bool preserve:{false,true})for(bool enabled:{false,true})
        {
            options.preservePitch=preserve;
            auto original=base;auto& src=original.tracks[0].clips[0].notes[0];src.utauFlagCurveEnabled=enabled;src.utauFlagSplit=enabled;
            auto data=original;const auto plan=hamood::analyse(data,options);hamood::generate(data,options,plan);
            const auto prefix=modeName+" pitch="+juce::String(preserve?1:0)+" flags="+juce::String(enabled?1:0)+" ";
            if(data.tracks.size()!=2){check(prefix+"generates harmony",false);continue;}
            const auto& dst=data.tracks[1];const auto& n=dst.clips[0].notes[0];
            check(prefix+"mode voicebank global flags and velocity retained",dst.utauMode==mode&&!trackIsDiffSinger(dst)&&dst.voicebankDirectory==bank&&dst.utauGlobalFlags==track.utauGlobalFlags&&dst.utauConsonantVelocity==125);
            check(prefix+"all textual regional and shaped curve data retained",sameFlags(src,n)&&n.diffSingerTiming==src.diffSingerTiming);
            check(prefix+"timing OTO split and splice retained",n.utauOto==src.utauOto&&n.utauStpSeconds==src.utauStpSeconds&&n.utauConsonantVelocity==src.utauConsonantVelocity&&n.utauJieSplitSet&&n.utauJieSplit1==.1&&n.utauJieSplit2==.3&&n.utauJieSplit3==.9&&n.utauSplice);
            const auto before=AudioEngine::diagnosticUtauRequestNotes(original,"clip",{"a-note"});
            const auto after=AudioEngine::diagnosticUtauRequestNotes(data,dst.clips[0].id,{n.id});
            const auto& a=before.front();const auto& b=after.front();
            check(prefix+"renderer receives same flags and precedence",a.flags==b.flags&&b.flags==src.utauFlags+track.utauGlobalFlags&&a.regionFlags==b.regionFlags&&a.flagSplit==b.flagSplit);
            check(prefix+"renderer receives every curve and enable state",a.flagCurve==b.flagCurve&&a.flagCurves==b.flagCurves&&b.flagCurves.size()==flagCurveKinds().size()+1);
            check(prefix+"renderer receives timing and OTO unchanged",a.oto==b.oto&&a.jieSplit==b.jieSplit&&a.jieSplitSet==b.jieSplitSet&&a.consonantVelocity==b.consonantVelocity&&a.preutteranceSeconds==b.preutteranceSeconds&&a.overlapSeconds==b.overlapSeconds&&a.stpSeconds==b.stpSeconds);
            ProjectModel model;model.replace(original);model.replace(data);const auto file=directory.getChildFile(modeName+"-"+juce::String(preserve?1:0)+"-"+juce::String(enabled?1:0)+".hjpx");juce::String error;
            ProjectModel reopened;const bool saved=model.save(file,error)&&reopened.load(file,error);
            check(prefix+"flags and mode survive save/reopen",saved&&sameFlags(src,reopened.snapshot().tracks[1].clips[0].notes[0])&&reopened.snapshot().tracks[1].utauMode==mode);
            model.undo();check(prefix+"undo preserves source flags",model.snapshot().tracks.size()==1&&sameFlags(src,model.snapshot().tracks[0].clips[0].notes[0]));
            model.redo();check(prefix+"redo preserves harmony flags",model.snapshot().tracks.size()==2&&sameFlags(src,model.snapshot().tracks[1].clips[0].notes[0]));
        }
        // Real synthesis of a HAMOOD-created note, using all three flag layers.
        auto audible=base;auto& source=audible.tracks[0].clips[0].notes[0];
        source.utauFlags="K1Mj8Mb20";audible.tracks[0].utauGlobalFlags="g0";
        source.utauFlagCurves={{"g",{{0,0},{.4,15},{1,30}}},{"Mb",{{0,10},{.5,50},{1,80}}}};
        source.utauRegionFlags1="Mb0";source.utauRegionFlags2="";source.utauRegionFlags3="Mb60";source.utauRegionFlags4="Mb20";
        source.utauConsonantVelocity=inheritedUtauConsonantVelocity;
        options.preservePitch=true;auto plan=hamood::analyse(audible,options);hamood::generate(audible,options,plan);
        const auto& harmony=audible.tracks[1];
        backend::UtauRenderRequest request;request.voicebankDirectory=bank;request.resamplerExecutable=resampler;
        request.fourRegion=utauModeUsesRegions(mode);request.consonantClasses=mode==UtauMode::mou;request.targetDurationSeconds=1.6;request.bpm=120;
        request.notes=AudioEngine::diagnosticUtauRequestNotes(audible,harmony.clips[0].id);
        check(modeName+" inherited velocity reaches renderer",request.notes[0].consonantVelocity==125);
        auto result=backend::UtauRenderer::render(request);
        const auto sounds=result.buffer.getNumSamples()>1000&&result.buffer.getMagnitude(0,result.buffer.getNumSamples())>.001f;
        check(modeName+" real WCSNDM synthesis with text/region/linear flags",sounds&&result.warning.isEmpty());
        std::cout<<"render backend "<<result.backend<<" warning "<<result.warning<<std::endl;
        if(sounds)
        {
            auto output=directory.getChildFile("HAMOOD-"+modeName+"-flags.wav").createOutputStream();
            if(output){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(output.release(),result.sampleRate,(unsigned int)result.buffer.getNumChannels(),16,{},0));if(writer)writer->writeFromAudioSampleBuffer(result.buffer,0,result.buffer.getNumSamples());}
        }
    }
    auto* report=new juce::DynamicObject();report->setProperty("ok",ok);report->setProperty("count",count);report->setProperty("checks",checks);
    directory.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report)));return ok;
}
}
