#pragma once
#include "../AudioEngine.h"
#include "../backend/NsfHifiganRenderer.h"
#include "../backend/AmplitudeEnvelopeCurve.h"

namespace hachi {
inline bool runNsfProjectNoteSmoke(const juce::File& folder, const juce::File& models,
    const juce::File& projectFile, const juce::String& noteId)
{
    using namespace backend;
    folder.createDirectory(); bool ok=true;
    juce::Array<juce::var> checks;
    const auto check=[&](const char* name,bool passed) {
        auto* row=new juce::DynamicObject(); row->setProperty("name",name);row->setProperty("passed",passed);
        checks.add(juce::var(row));ok &= passed;
    };
    auto* detail=new juce::DynamicObject();detail->setProperty("version",JUCE_APPLICATION_VERSION_STRING);
    try {
        ProjectModel model;juce::String error;
        if (!model.load(projectFile,error)) throw std::runtime_error(error.toStdString());
        const auto data=model.snapshot(); bool found=false;
        for (const auto& track:data.tracks) for (const auto& clip:track.clips) {
            const auto at=std::find_if(clip.notes.begin(),clip.notes.end(),[&](const auto& n){return n.id==noteId;});
            if (at==clip.notes.end()) continue;
            found=true;
            const auto index=static_cast<std::size_t>(at-clip.notes.begin());
            auto notes=AudioEngine::utauRequestNotesForClip(data,clip.id);
            if (index>=notes.size()) throw std::runtime_error("request note index missing");
            const auto original=notes[index];
            detail->setProperty("note_id",noteId);detail->setProperty("lyric",original.alias);
            detail->setProperty("start_seconds",original.startSeconds);detail->setProperty("duration_seconds",original.durationSeconds);
            detail->setProperty("saved_envelope_end",original.amplitudeEnvelope.empty()?0:original.amplitudeEnvelope.back().timeSeconds);
            const auto first=index>0?index-1:index;const auto last=std::min(notes.size(),index+2);
            UtauRenderRequest request;request.voicebankDirectory=track.voicebankDirectory;
            request.fourRegion=utauModeUsesRegions(track.utauMode);request.consonantClasses=track.utauMode==UtauMode::mou;
            request.bpm=data.bpm;request.notes.assign(notes.begin()+static_cast<std::ptrdiff_t>(first),notes.begin()+static_cast<std::ptrdiff_t>(last));
            const auto pieceIndex=index-first;
            request.targetDurationSeconds=request.notes.back().startSeconds+request.notes.back().durationSeconds+.2;
            juce::AudioBuffer<float> raw,piece,oldPiece;double rate=0,lead=0;
            request.notePiece=[&](std::size_t i,const auto& audio,double sampleRate,double pre,const auto& gainAt,const auto&) {
                if(i!=pieceIndex)return;rate=sampleRate;lead=pre;raw=audio;piece=audio;oldPiece=audio;
                const auto oldGainAt=[&](double time) {
                    const auto& points=original.amplitudeEnvelope;if(points.empty())return 1.0f;
                    if(time<=points.front().timeSeconds)return envelopeGainFromDb(points.front().gainDb);
                    for(std::size_t j=1;j<points.size();++j)if(time<=points[j].timeSeconds) {
                        const auto& a=points[j-1];const auto& b=points[j];
                        const auto u=static_cast<float>((time-a.timeSeconds)/std::max(1.e-9,b.timeSeconds-a.timeSeconds));
                        return envelopeGainFromDb(envelopeDbBetween(a.gainDb,b.gainDb,u,a.linearToNext));
                    }
                    return envelopeGainFromDb(points.back().gainDb);
                };
                for(int sample=0;sample<audio.getNumSamples();++sample) {
                    const auto time=sample/sampleRate-pre;
                    for(int c=0;c<audio.getNumChannels();++c) {
                        piece.setSample(c,sample,audio.getSample(c,sample)*gainAt(time));
                        oldPiece.setSample(c,sample,audio.getSample(c,sample)*oldGainAt(time));
                    }
                }
            };
            const auto result=renderNsfUtauPhrase(request,models,{});
            check("actual_project_native_render_succeeds",result.backend=="hifisampler-native-voicebank"&&result.warning.isEmpty()&&piece.getNumSamples()>0);
            detail->setProperty("warning",result.warning);
            const auto localEnd=piece.getNumSamples()/std::max(1.0,rate)-lead;
            detail->setProperty("sounding_end_seconds",localEnd);
            const auto probe=std::min(original.durationSeconds*.85,localEnd-.10);
            const auto sample=static_cast<int>((lead+probe)*rate);const auto count=static_cast<int>(.04*rate);
            const auto valid=count>0&&sample>=0&&sample+count<=piece.getNumSamples();
            const auto oldRms=valid?oldPiece.getRMSLevel(0,sample,count):0;
            const auto rawRms=valid?raw.getRMSLevel(0,sample,count):0;
            const auto newRms=valid?piece.getRMSLevel(0,sample,count):0;
            detail->setProperty("probe_seconds",probe);detail->setProperty("raw_rms",rawRms);
            detail->setProperty("old_envelope_rms",oldRms);detail->setProperty("fitted_envelope_rms",newRms);
            check("saved_short_envelope_reproduces_reported_silence",valid&&oldRms<1.e-7f&&rawRms>1.e-5f);
            check("fitted_envelope_preserves_actual_shang_latter_half",valid&&newRms>1.e-5f&&newRms>rawRms*.5f);
            const auto mixedStart=static_cast<int>((original.startSeconds+probe)*result.sampleRate);
            check("final_phrase_mix_contains_shang_latter_half",mixedStart+count<=result.buffer.getNumSamples()
                &&result.buffer.getRMSLevel(0,mixedStart,count)>1.e-5f);
            const auto resolved=UtauRenderer::resolveVoiceSample(request.voicebankDirectory,original.alias,original.midiNote,
                original.consonantVelocity,request.fourRegion,request.consonantClasses,original.stpSeconds,
                original.preutteranceOverrideEnabled,original.preutteranceSeconds,original.overlapOverrideEnabled,original.overlapSeconds,&original.oto);
            detail->setProperty("sample_file",resolved.file.getFullPathName());
            NsfUtauSampleTiming timing { resolved.offsetSeconds,resolved.endSeconds,resolved.consonantSeconds,
                resolved.preutteranceSeconds,resolved.overlapSeconds,resolved.fileSeconds,resolved.hasRegions,
                resolved.regionSeconds,resolved.mouClasses };
            const auto plan=buildNsfUtauNotePlan(timing,original.startSeconds,original.durationSeconds,
                std::pow(2.0,1.0-juce::jlimit(0,200,original.consonantVelocity)/100.0),0,
                original.jieSplitSet?&original.jieSplit:nullptr,
                UtauRenderer::readsOnlyFirstTwoRegions(request.fourRegion,request.consonantClasses,original.durationSeconds),
                timing.hasRegions?localEnd+lead:0);
            detail->setProperty("effective_preutterance_seconds",lead);
            detail->setProperty("region_classes",plan.regionClasses);
            juce::Array<juce::var> regionTiming;
            for(std::size_t i=1;i<plan.timeMap.size();++i) {
                auto* span=new juce::DynamicObject();
                span->setProperty("source_seconds",plan.timeMap[i].sourceSeconds-plan.timeMap[i-1].sourceSeconds);
                span->setProperty("output_seconds",plan.timeMap[i].targetSeconds-plan.timeMap[i-1].targetSeconds);
                regionTiming.add(juce::var(span));
            }
            detail->setProperty("region_timing",juce::var(regionTiming));
            const auto fitted=UtauRenderer::fitAmplitudeEnvelope(original.amplitudeEnvelope,lead,localEnd);
            check("release_ramp_keeps_its_duration",fitted.size()==original.amplitudeEnvelope.size()&&fitted.size()>=4
                &&std::abs((fitted.back().timeSeconds-fitted[fitted.size()-2].timeSeconds)
                    -(original.amplitudeEnvelope.back().timeSeconds-original.amplitudeEnvelope[original.amplitudeEnvelope.size()-2].timeSeconds))<1.e-8);
            const auto save=[&](const char* name,const auto& audio,double sampleRate) {
                juce::WavAudioFormat format;auto stream=folder.getChildFile(name).createOutputStream();
                std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),sampleRate,
                    static_cast<unsigned int>(audio.getNumChannels()),24,{},0));
                if(writer)writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());
            };
            if(piece.getNumSamples()>0){save("shang-raw.wav",raw,rate);save("shang-old-envelope.wav",oldPiece,rate);save("shang-fixed.wav",piece,rate);}
            if(result.buffer.getNumSamples()>0)save("shang-with-neighbors.wav",result.buffer,result.sampleRate);
        }
        check("requested_note_found",found);
    }catch(const std::exception& e){check(e.what(),false);}
    detail->setProperty("ok",ok);detail->setProperty("checks",juce::var(checks));
    folder.getChildFile("project-note-validation.json").replaceWithText(juce::JSON::toString(juce::var(detail),true));return ok;
}
}
