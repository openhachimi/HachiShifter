#pragma once
#include "../backend/Mld5Renderer.h"
#include "../backend/McpServer.h"
#include <iostream>
namespace hachi
{
inline bool runMld5RecoveredSmoke(const juce::File& root,const juce::File& projectFile={})
{
    root.createDirectory();bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    const auto wave=[](int rate,int channels){juce::AudioBuffer<float> pcm(channels,rate);for(int i=0;i<rate;++i){double x=0;for(int h=1;h<=8;++h)x+=.18/h*std::sin(2.*juce::MathConstants<double>::pi*261.6255653*h*i/rate);for(int c=0;c<channels;++c)pcm.setSample(c,i,float(x*(c?.6:1.)));}return pcm;};
    const auto finite=[](const juce::AudioBuffer<float>& pcm){if(pcm.getNumSamples()<1)return false;double energy=0;for(int c=0;c<pcm.getNumChannels();++c)for(int i=0;i<pcm.getNumSamples();++i){const auto v=pcm.getSample(c,i);if(!std::isfinite(v))return false;energy+=v*v;}return energy>1.e-8;};
    const auto pitch=[](const juce::AudioBuffer<float>& pcm,int rate){int crossings=0;const int a=int(.15*rate),b=int(.85*rate);for(int i=a+1;i<b&&i<pcm.getNumSamples();++i)if(pcm.getSample(0,i-1)<=0&&pcm.getSample(0,i)>0)++crossings;return crossings/.7;};
    backend::Mld5Renderer renderer;
    for(int rate:{32000,44100,48000})for(int channels:{1,2})
    {
        auto pcm=wave(rate,channels);backend::Mld5RenderRequest request;request.input=&pcm;request.sampleRate=rate;request.targetSamples=rate;request.sourceMidi.assign(201,60);request.targetMidi.assign(201,65);request.noteGain.assign(201,1);request.formantSemitones.assign(201,0);request.timeMap={{0,0},{.08,.08},{.72,.72},{1,1}};
        const auto output=renderer.render(request,&error);check("recovered_output_has_exact_rate_grid_and_channels",output.getNumSamples()==rate&&output.getNumChannels()==channels&&finite(output));
        if(output.getNumSamples()!=rate)continue;
        const auto expected=261.6255653*std::exp2(5./12.);check("constant_pitch_edit_is_not_old_or_source_passthrough",std::abs(pitch(output,rate)-expected)<expected*.025);
        request.noteGain.assign(201,.5f);const auto half=renderer.render(request,&error);double gainError=0;if(half.getNumSamples()==rate)for(int i=0;i<rate;++i)gainError=std::max(gainError,std::abs(double(half.getSample(0,i)-output.getSample(0,i)*.5f)));else gainError=1;
        check("note_gain_is_applied_exactly_once",gainError<1.e-6);
        if(channels==2){double difference=0;for(int i=rate/10;i<rate*9/10;++i)difference=std::max(difference,std::abs(double(output.getSample(1,i)-output.getSample(0,i)*.6f)));check("stereo_channel_relationship_is_preserved",difference<.005);}
        request.noteGain.assign(201,1);request.formantSemitones.assign(201,3);const auto formant=renderer.render(request,&error);double delta=0;if(formant.getNumSamples()==rate)for(int i=0;i<rate;++i)delta+=std::abs(double(formant.getSample(0,i)-output.getSample(0,i)));check("formant_Q_control_reaches_recovered_spectral_frames",finite(formant)&&delta>.01);
        request.sourceMidi.assign(201,0);request.targetMidi.assign(201,0);request.formantSemitones.assign(201,0);const auto uv=renderer.render(request,&error);double uvError=0;if(uv.getNumSamples()==rate)for(int i=0;i<rate;++i)uvError=std::max(uvError,std::abs(double(uv.getSample(0,i)-pcm.getSample(0,i))));else uvError=1;
        check("unvoiced_source_does_not_acquire_fake_F0",uvError<1.e-6);
    }
    auto pcm=wave(48000,2);backend::Mld5RenderRequest dynamic;dynamic.input=&pcm;dynamic.sampleRate=48000;dynamic.targetSamples=38400;dynamic.sourceMidi.resize(161);dynamic.targetMidi.resize(161);dynamic.formantSemitones.resize(161);dynamic.noteGain.resize(161,1);dynamic.timeMap={{0,0},{.2,.12},{.55,.6},{.8,1}};
    for(int i=0;i<161;++i){const bool voiced=i>=20&&i<140;dynamic.sourceMidi[i]=voiced?60.f:0.f;dynamic.targetMidi[i]=voiced?float(65+2*std::sin(i*.1)):0.f;dynamic.formantSemitones[i]=float(std::sin(i*.04));}
    const auto rendered=renderer.render(dynamic,&error);check("dynamic_target_pitch_Q_UV_and_nonlinear_source_clock",rendered.getNumSamples()==38400&&rendered.getNumChannels()==2&&finite(rendered));
    dynamic.timeMap={{0,0},{.5,.6},{.8,.4}};check("reversed_source_clock_is_rejected_without_fallback",renderer.render(dynamic,&error).getNumSamples()==0&&error.isNotEmpty());
    dynamic.framePeriodMs=1.e-300;check("extreme_control_rate_is_rejected_before_integer_conversion",renderer.render(dynamic,&error).getNumSamples()==0&&error.contains("sample/control"));
    auto uvFixture=wave(48000,1);backend::Mld5RenderRequest uvRequest;uvRequest.input=&uvFixture;uvRequest.sampleRate=48000;uvRequest.framePeriodMs=5;uvRequest.sourceMidi={0,60,60,0};uvRequest.targetMidi={0,65,65,0};uvRequest.formantSemitones={0,0,0,0};
    const auto uvControl=backend::Mld5Renderer::diagnosticControlAt(uvRequest,.0025);const auto wantedRatio=std::exp2(5./12.);
    check("UV_gate_does_not_open_at_unity_before_pitch_Q_activation",std::abs(uvControl.pitchRatio-wantedRatio)<1.e-6&&std::abs(uvControl.probeRatio-wantedRatio)<1.e-6&&std::abs(uvControl.voicingGain-.5f)<1.e-6);
    ProjectModel model;const auto selected=model.addTrack("MLD5",true),other=model.addTrack("Unchanged default",true);backend::McpServer mcp(model,nullptr);auto* args=new juce::DynamicObject();args->setProperty("track_id",selected);args->setProperty("pitch_algorithm","mld5");const auto accepted=mcp.executeTool("set_track",juce::var(args));bool selectedMld5=false,otherDefault=false;
    for(const auto& track:model.snapshot().tracks){if(track.id==selected)selectedMld5=track.pitchAlgorithm==PitchAlgorithm::mld5;if(track.id==other)otherDefault=track.pitchAlgorithm==PitchAlgorithm::nsfHifigan;}
    check("MCP_selects_MLD5_without_changing_default_or_other_track",!bool(accepted.getProperty("isError",false))&&selectedMld5&&otherDefault);
    const auto selectedFingerprint=model.contentFingerprint();auto* denied=new juce::DynamicObject();denied->setProperty("track_id",selected);denied->setProperty("pitch_algorithm","mld3");const auto refusal=mcp.executeTool("set_track",juce::var(denied));check("MLD3_stays_disabled",bool(refusal.getProperty("isError",false))&&model.contentFingerprint()==selectedFingerprint);
    ProjectModel cold;const auto saved=root.getChildFile("mld5-selected.hjpx");bool persisted=model.save(saved,error)&&cold.load(saved,error);for(const auto& track:cold.snapshot().tracks)if(track.id==selected)persisted&=track.pitchAlgorithm==PitchAlgorithm::mld5;check("MLD5_saved_algorithm_string_remains_loadable",persisted);
    if(projectFile!=juce::File{})
    {
        const auto before=juce::SHA256(projectFile).toHexString();ProjectModel actual;const bool loaded=actual.load(projectFile,error);check("provided_project_loads_read_only",loaded);bool exercised=false;
        if(loaded)for(auto track:actual.snapshot().tracks)
        {
            if(exercised||!trackShowsAllNativeRegions(track)||track.accompaniment)continue;track.pitchAlgorithm=PitchAlgorithm::mld5;
            for(const auto& clip:track.clips)
            {
                if(exercised||!clip.sourceFile.existsAsFile()||!clip.parts.empty())continue;
                auto request=AudioEngine::diagnosticNativeRequest(clip,track);bool voiced=false;
                for(std::size_t i=0;i<request.targetMidi.size()&&i<request.sourceMidi.size();++i)if(request.sourceMidi[i]>0){request.targetMidi[i]=request.sourceMidi[i]+5;voiced=true;}
                if(!voiced)continue;backend::RenderService service;struct Completion{juce::WaitableEvent done;backend::RenderedAudio output;};auto state=std::make_shared<Completion>();
                service.renderMld5File(request,[state](auto result){state->output=std::move(result);state->done.signal();});const bool finished=state->done.wait(60000);
                if(!finished){check("actual_vocal_source_render_completed",false);exercised=true;break;}auto output=std::move(state->output);
                check("actual_vocal_source_edited_through_recovered_service",finished&&output.backend.startsWith(backend::Mld5Renderer::implementationId)&&finite(output.buffer)&&output.buffer.getNumSamples()==int(std::llround(request.targetDurationSeconds*output.sampleRate)));
                log+="actual_source="+clip.sourceFile.getFullPathName()+"; backend="+output.backend+"; warning="+output.warning+"\n";
                if(finished&&output.buffer.getNumSamples()>0){juce::WavAudioFormat wav;auto stream=root.getChildFile("actual-vocal-up5.wav").createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),output.sampleRate,unsigned(output.buffer.getNumChannels()),24,{},0));if(writer)writer->writeFromAudioSampleBuffer(output.buffer,0,output.buffer.getNumSamples());}exercised=true;
            }
        }
        check("actual_vocal_source_was_found",exercised);check("provided_project_bytes_unchanged",juce::SHA256(projectFile).toHexString()==before);
    }
    root.getChildFile("verification.txt").replaceWithText(log);return ok;
}
}
