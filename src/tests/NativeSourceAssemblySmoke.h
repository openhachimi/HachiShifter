#pragma once
#include "../backend/RenderService.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include <algorithm>
#include <cmath>

namespace hachi::tests
{
// Deliberately not registered in Main: the parent/audio harness owns invocation.
// Real NSF inference, in-memory sources only. Missing model is a failure, not a
// WORLD fallback. Output is written only to the explicitly supplied test folder.
inline bool runNativeSourceAssemblySmoke(const juce::File& folder, const juce::File& model)
{
    folder.createDirectory();
    juce::StringArray failures;
    const auto check=[&](bool ok,const juce::String& message){if(!ok)failures.add(message);};
    constexpr double rate=48000.;
    auto first=std::make_shared<juce::AudioBuffer<float>>(1,24000);
    auto second=std::make_shared<juce::AudioBuffer<float>>(1,24000);
    for(int i=0;i<24000;++i)
    {
        first->setSample(0,i,.12f*std::sin(float(juce::MathConstants<double>::twoPi*220*i/rate)));
        second->setSample(0,i,.09f*std::sin(float(juce::MathConstants<double>::twoPi*330*i/rate)));
    }
    const auto stamp=[](const auto& pcm){return juce::SHA256(pcm.getReadPointer(0),std::size_t(pcm.getNumSamples())*sizeof(float)).toHexString();};
    const auto firstHash=stamp(*first),secondHash=stamp(*second);
    auto assembly=std::make_shared<backend::NsfHifiganSourceAssembly>();
    backend::NsfHifiganSourcePart a,b;
    a.source=first;a.sampleRate=rate;a.supportFrom=0;a.supportTo=.5;
    a.timeMap={{0,0},{.5,.5}};a.sourceMidiAt=[](double){return 57.f;};
    a.gainAt=[](double t){return float(t<=.3?1.:std::clamp((.5-t)/.2,0.,1.));};
    b.source=second;b.sampleRate=rate;b.supportFrom=.3;b.supportTo=.8;
    b.timeMap={{.3,0},{.8,.5}};b.sourceMidiAt=[](double){return 64.f;};
    b.gainAt=[](double t){return float(t>=.5?1.:std::clamp((t-.3)/.2,0.,1.));};
    assembly->push_back(a);assembly->push_back(b);
    for(int i=0;i<=200;++i){const auto t=.3+.2*i/200.;check(std::abs(a.gainAt(t)+b.gainAt(t)-1.f)<1.e-6f,"Complementary source weights");}
    backend::Mld5FileRenderRequest request;
    request.sourceBuffer=first;request.sourceRate=rate;request.sourceDurationSeconds=.5;
    request.targetDurationSeconds=.8;request.targetMidi.assign(161,60.f);
    request.hifiganModelDirectory=model;request.pitchBackend=backend::PitchRenderBackend::nsfHifigan;
    request.stretchAlgorithm=1;request.sourceAssembly=assembly;request.nsfNoiseProtection=false;
    backend::RenderService service;
    const auto render=[&](backend::Mld5FileRenderRequest req)
    {
        auto done=std::make_shared<juce::WaitableEvent>();
        auto result=std::make_shared<backend::RenderedAudio>();
        service.renderMld5File(std::move(req),[done,result](backend::RenderedAudio value){*result=std::move(value);done->signal();});
        check(done->wait(120000),"Render timed out");
        return result;
    };
    auto joined=render(request);
    check(joined->buffer.getNumSamples()==38400,"Assembly output uses group target sample count");
    check(joined->backend.contains("source-assembly"),"Actual NSF source-assembly backend marker");
    check(firstHash==stamp(*first)&&secondHash==stamp(*second),"Source PCM stays immutable");
    // WORLD uses the same source assembly contract and one synthesis pass;
    // it must not quietly fall back to per-source rendering.
    request.pitchBackend=backend::PitchRenderBackend::world;
    const auto world=render(request);
    check(world->buffer.getNumSamples()==38400&&world->backend.contains("source-assembly"),"WORLD source assembly uses one group route");
    request.pitchBackend=backend::PitchRenderBackend::nsfHifigan;
    request.sourceBuffer=std::make_shared<const juce::AudioBuffer<float>>();
    const auto invalid=render(request);
    check(invalid->buffer.getNumSamples()==0&&invalid->warning.contains("in-memory"),"Invalid memory source never falls back to disk");
    if(joined->buffer.getNumSamples()>0)
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream(folder.getChildFile("nsf-source-assembly.wav").createOutputStream());
        if(stream)
        {
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),rate,
                static_cast<unsigned int>(joined->buffer.getNumChannels()),24,{},0));
            check(writer&&writer->writeFromAudioSampleBuffer(joined->buffer,0,joined->buffer.getNumSamples()),"Write test output");
        }
        else check(false,"Create test output");
    }
    folder.getChildFile("source-assembly-result.txt").replaceWithText(failures.isEmpty()?"PASS\n":failures.joinIntoString("\n"));
    return failures.isEmpty();
}
}
