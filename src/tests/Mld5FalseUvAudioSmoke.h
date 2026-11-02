#pragma once
#include "../backend/Mld5Renderer.h"
#include <iostream>
namespace hachi
{
inline bool runMld5FalseUvAudioSmoke(const juce::File& out)
{
    out.createDirectory();bool ok=true;juce::String log,error;const auto check=[&](const char* name,bool pass){ok&=pass;log<<name<<"="<<(pass?"PASS":"FAIL")<<" "<<error<<"\n";std::cout<<name<<"="<<pass<<std::endl;error.clear();};
    constexpr double fs=48000,sourceHz=420;const float sourceMidi=float(69+12*std::log2(sourceHz/440.));const double targetHz=440*std::exp2((79.-69)/12.);
    juce::AudioBuffer<float> source(1,48000);for(int i=0;i<48000;++i)source.setSample(0,i,float(.04*std::sin(2*juce::MathConstants<double>::pi*sourceHz*i/fs)));
    backend::Mld5RenderRequest request;request.input=&source;request.sampleRate=fs;request.framePeriodMs=5;request.sourceMidi.assign(201,sourceMidi);request.targetMidi.assign(201,79);request.timeMap={{0,0},{1,1}};
    for(int frame=90;frame<=110;++frame)request.sourceMidi[std::size_t(frame)]=request.targetMidi[std::size_t(frame)]=0;
    backend::Mld5Renderer renderer;const auto legacy=renderer.render(request,&error);check("legacy_false_UV_control_renders_negative_reference",legacy.getNumSamples()==48000);
    request.sourceMidiHint.assign(201,sourceMidi);request.targetMidiIntent.assign(201,79);int recovered=0;request.sourceRecoveryObserver=[&](int count){recovered=count;};
    const auto corrected=renderer.render(request,&error);check("known_target_intent_recovers_missing_vowel_from_actual_PCM",corrected.getNumSamples()==48000&&recovered>=20);
    const auto frequency=[](const juce::AudioBuffer<float>& pcm,int first,int last){if(pcm.getNumSamples()<last)return 0.;std::vector<double> positions;for(int i=first+1;i<last;++i){const auto a=pcm.getSample(0,i-1),b=pcm.getSample(0,i);if(a<=0&&b>0)positions.push_back(i-1+double(-a)/(b-a));}return positions.size()>5?48000.*double(positions.size()-1)/(positions.back()-positions.front()):0.;};
    const auto beforeHz=frequency(legacy,23040,24960),afterHz=frequency(corrected,23040,24960);
    log+="before_gap_hz="+juce::String(beforeHz,9)+"; after_gap_hz="+juce::String(afterHz,9)+"; intended_hz="+juce::String(targetHz,9)+"; recovered_frames="+juce::String(recovered)+"\n";
    check("negative_reference_really_falls_to_original_420Hz",std::abs(beforeHz-sourceHz)<.2);
    check("corrected_actual_audio_stays_at_authored_target_not_source_pitch",afterHz>0&&std::abs(1200*std::log2(afterHz/targetHz))<5.);
    check("original_request_masks_are_not_edited",request.sourceMidi[100]==0&&request.targetMidi[100]==0);
    // A caller-declared unpitched source does not gain permission from a hint-free
    // acoustic sine. The generic detector must never invent a target note.
    auto unpitched=request;unpitched.sourceMidi.assign(201,0);unpitched.targetMidi.assign(201,0);unpitched.sourceMidiHint.clear();unpitched.targetMidiIntent.clear();unpitched.sourceRecoveryObserver={};
    const auto dry=renderer.render(unpitched,&error);bool identical=dry.getNumSamples()==source.getNumSamples();if(identical)for(int i=0;i<48000;++i)identical&=dry.getSample(0,i)==source.getSample(0,i);
    check("explicit_unpitched_without_intent_stays_original_PCM",identical);
    return out.getChildFile("verification.txt").replaceWithText(log)&&ok;
}
}
