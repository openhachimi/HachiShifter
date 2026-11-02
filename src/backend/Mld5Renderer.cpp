#include "Mld5Renderer.h"
#include "output_gain_control.hpp"
#include "Mld5NativeControl.h"
#include "Mld5SourcePitchRecovery.h"
#include "primitives.hpp"
#include <juce_audio_utils/juce_audio_utils.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hachi::backend
{
namespace
{
constexpr double renderRate=48000.;
float curve(const std::vector<float>& values,double position,float fallback=0.f,bool voiced=false)
{
    if(values.empty())return fallback;
    position=std::clamp(position,0.,double(values.size()-1));const auto i=std::size_t(position);const auto j=std::min(i+1,values.size()-1);
    if(voiced&&(values[i]<=0||values[j]<=0))return values[i]; // no interpolated fake F0 across UV
    return values[i]+float(position-double(i))*(values[j]-values[i]);
}
juce::AudioBuffer<float> resample(const juce::AudioBuffer<float>& input,double rate,double outputRate,int count)
{
    juce::AudioBuffer<float> output(input.getNumChannels(),count);output.clear();
    if(rate==outputRate){for(int c=0;c<input.getNumChannels();++c)output.copyFrom(c,0,input,c,0,std::min(count,input.getNumSamples()));return output;}
    juce::AudioBuffer<float> owned(input);juce::MemoryAudioSource memory(owned,false);
    juce::ResamplingAudioSource converter(&memory,false,input.getNumChannels());converter.setResamplingRatio(rate/outputRate);converter.prepareToPlay(4096,outputRate);
    for(int offset=0;offset<count;offset+=4096)converter.getNextAudioBlock({&output,offset,std::min(4096,count-offset)});
    converter.releaseResources();return output;
}
struct Controls
{
    const Mld5RenderRequest& request;
    std::vector<TimeMapPoint> map;
    double frameSeconds,sourceDuration,targetDuration;
    std::vector<float> pitchCarrier,probeCarrier;
    explicit Controls(const Mld5RenderRequest& value,int output):request(value),map(value.timeMap),frameSeconds(value.framePeriodMs*.001),sourceDuration(value.input->getNumSamples()/value.sampleRate),targetDuration(output/value.sampleRate)
    {
        if(map.empty())map={{0,0},{targetDuration,sourceDuration}};
        if(map.size()<2)throw std::runtime_error("MLD5 requires at least two source-time anchors");
        for(std::size_t i=0;i<map.size();++i)
        {
            const auto& p=map[i];if(!std::isfinite(p.targetSeconds)||!std::isfinite(p.sourceSeconds)||p.sourceSeconds<0||p.sourceSeconds>sourceDuration+1./request.sampleRate+1.e-9||(i&&(p.targetSeconds<=map[i-1].targetSeconds||p.sourceSeconds<=map[i-1].sourceSeconds)))throw std::runtime_error("MLD5 requires a finite strictly forward source clock");
        }
    }
    double mapped(double value,bool inverse=false) const
    {
        const auto x=[&](const auto& p){return inverse?p.sourceSeconds:p.targetSeconds;};const auto y=[&](const auto& p){return inverse?p.targetSeconds:p.sourceSeconds;};
        auto hi=std::upper_bound(map.begin(),map.end(),value,[&](double v,const auto& p){return v<x(p);});
        if(hi==map.begin())hi=std::next(map.begin());if(hi==map.end())hi=std::prev(map.end());const auto& a=*std::prev(hi);const auto& b=*hi;
        return y(a)+(value-x(a))*(y(b)-y(a))/(x(b)-x(a));
    }
    float source(double t)const{return curve(request.sourceMidi,t/frameSeconds,0,true);}
    float target(double t)const{return curve(request.targetMidi,t/frameSeconds,source(t),true);}
    bool voiced(double t)const{return source(t)>0&&target(t)>0;}
    void preparePitchCarrier()
    {
        const auto count=std::max<std::size_t>(2,std::max(request.sourceMidi.size(),request.targetMidi.size()));
        pitchCarrier.assign(count,std::numeric_limits<float>::quiet_NaN());probeCarrier=pitchCarrier;
        for(std::size_t i=0;i<count;++i){const auto t=i*frameSeconds;if(voiced(t)){pitchCarrier[i]=float(std::exp2(double(target(t)-source(t))/12.));probeCarrier[i]=float(pitchCarrier[i]/std::exp2(double(curve(request.formantSemitones,double(i)))/12.));}}
        for(auto* lane:{&pitchCarrier,&probeCarrier})
        {
            float held=std::numeric_limits<float>::quiet_NaN();for(auto& value:*lane){if(std::isfinite(value))held=value;else if(std::isfinite(held))value=held;}
            held=1.f;for(auto it=lane->rbegin();it!=lane->rend();++it){if(std::isfinite(*it))held=*it;else *it=held;}
        }
        for(std::size_t i=0;i+1<count;++i)if(!voiced(i*frameSeconds)&&voiced((i+1)*frameSeconds))
        {pitchCarrier[i]=pitchCarrier[i+1];probeCarrier[i]=probeCarrier[i+1];}
        // Carrier controls prewarm the wet path through UV. The audible UV mask
        // stays untouched; never open the gate at unity then jump pitch/Q later.
    }
    double ratio(double t)const{return curve(pitchCarrier,t/frameSeconds,1.f);}
    float q(double t)const{return curve(probeCarrier,t/frameSeconds,1.f);}
    float gate(double t)const{const auto pos=t/frameSeconds,a=std::floor(pos),blend=pos-a;return float((voiced(a*frameSeconds)?1.-blend:0.)+(voiced((a+1)*frameSeconds)?blend:0.));}
    bool identity()const{for(const auto& p:map)if(p.sourceSeconds!=p.targetSeconds)return false;return true;}
};
float readOriginal(const juce::AudioBuffer<float>& pcm,int channel,double position)
{
    if(position<0||position>=pcm.getNumSamples())return 0;
    const auto first=int(position);const auto fraction=float(position-first);
    const auto at=[&](int index){return index>=0&&index<pcm.getNumSamples()?pcm.getSample(channel,index):0.f;};
    return melodyne_re::lagrange_cubic(at(first-1),at(first),at(first+1),at(first+2),fraction);
}
int recoverUnknownSource(Mld5RenderRequest& request,const Controls& controls)
{
    const auto size=request.sourceMidi.size();if(size==0||request.targetMidi.size()!=size||request.sourceMidiHint.size()!=size||request.targetMidiIntent.size()!=size)return 0;
    bool eligible=false;for(std::size_t i=0;i<size;++i)eligible|=request.sourceMidi[i]==0&&request.targetMidi[i]==0&&request.sourceMidiHint[i]>0&&request.targetMidiIntent[i]>0;
    if(!eligible)return 0;
    int channel=0;double strongest=-1;for(int c=0;c<request.input->getNumChannels();++c){double power=0;for(int i=0;i<request.input->getNumSamples();++i){const auto v=request.input->getSample(c,i);power+=double(v)*v;}if(power>strongest){strongest=power;channel=c;}}
    const auto* data=request.input->getReadPointer(channel);const std::vector<float> pcm(data,data+request.input->getNumSamples());
    const auto recovered=Mld5SourcePitchRecovery::recover(pcm,request.sampleRate,controls.frameSeconds,[&](double t){return controls.mapped(t);},request.sourceMidiHint,request.targetMidiIntent,request.sourceMidi,request.targetMidi);
    return int(recovered.size());
}
}

Mld5Renderer::ControlSnapshot Mld5Renderer::diagnosticControlAt(const Mld5RenderRequest& inputRequest,double seconds)
{
    auto request=inputRequest;
    if(!request.input||!std::isfinite(seconds)||request.framePeriodMs<=0)return {};
    Controls control(request,request.targetSamples>0?request.targetSamples:request.input->getNumSamples());recoverUnknownSource(request,control);control.preparePitchCarrier();
    return {control.ratio(seconds),control.q(seconds),control.gate(seconds)};
}

juce::AudioBuffer<float> Mld5Renderer::render(const Mld5RenderRequest& inputRequest,juce::String* error) const
{
    auto request=inputRequest; // only render-local acoustic evidence may be repaired
    if(error)error->clear();
    try
    {
        if(!request.input||request.input->getNumSamples()<1||request.input->getNumChannels()<1)throw std::runtime_error("MLD5 input is empty");
        if(!std::isfinite(request.sampleRate)||request.sampleRate<8000||request.sampleRate>384000||!std::isfinite(request.framePeriodMs)||request.framePeriodMs<.1||request.framePeriodMs>1000.)throw std::runtime_error("Invalid MLD5 sample/control rate");
        const auto count=request.targetSamples>0?request.targetSamples:request.input->getNumSamples();if(count>100000000)throw std::runtime_error("MLD5 output exceeds safety limit");
        for(const auto* values:{&request.sourceMidi,&request.targetMidi,&request.formantSemitones,&request.noteGain})for(float value:*values)if(!std::isfinite(value))throw std::runtime_error("Non-finite MLD5 control");
        for(int c=0;c<request.input->getNumChannels();++c)for(int s=0;s<request.input->getNumSamples();++s)if(!std::isfinite(request.input->getSample(c,s)))throw std::runtime_error("Non-finite MLD5 audio");
        const auto targetPoints=std::ceil((count/request.sampleRate)/(request.framePeriodMs*.001))+2;
        const auto sourcePoints=std::ceil((request.input->getNumSamples()/request.sampleRate)/(request.framePeriodMs*.001))+2;
        const auto principalRate=std::min(request.sampleRate,renderRate); // no forced 48k→32k→48k round trip
        const auto principalSamples=request.input->getNumSamples()*principalRate/request.sampleRate;
        const auto internalSamples=count*renderRate/request.sampleRate;
        if(!std::isfinite(targetPoints)||!std::isfinite(sourcePoints)||targetPoints>5000000||sourcePoints>5000000||principalSamples>100000000||internalSamples>100000000)throw std::runtime_error("MLD5 internal control/audio allocation exceeds safety limit");
        Controls controls(request,count);const auto recovered=recoverUnknownSource(request,controls);if(request.sourceRecoveryObserver)request.sourceRecoveryObserver(recovered);
        controls.preparePitchCarrier();const auto workCount=std::max(1,int(std::llround(internalSamples)));
        const auto controlCount=std::max<std::size_t>(2,std::size_t(std::ceil(controls.targetDuration/controls.frameSeconds))+1);
        bool anyVoiced=false,allVoiced=true,constantQ=true;const auto q0=controls.q(0);
        for(std::size_t i=0;i<controlCount;++i){const auto t=std::min(controls.targetDuration,i*controls.frameSeconds);anyVoiced|=controls.voiced(t);allVoiced&=controls.voiced(t);constantQ&=std::abs(controls.q(t)-q0)<1.e-6f;if(controls.ratio(t)<1./16||controls.ratio(t)>16||controls.q(t)<1./16||controls.q(t)>16)throw std::runtime_error("MLD5 pitch/formant ratio outside supported range");}
        juce::AudioBuffer<float> rendered(request.input->getNumChannels(),count);rendered.clear();
        if(anyVoiced)
        {
            const auto pcm=resample(*request.input,request.sampleRate,principalRate,std::max(1,int(std::llround(controls.sourceDuration*principalRate))));
            // Library P is source samples on SOURCE seconds, whereas Hachi's
            // sourceMidi is already sampled on TARGET time. Invert the clock
            // once here for P; never warp target controls a second time.
            const auto periodCount=std::max<std::size_t>(2,std::size_t(std::ceil(controls.sourceDuration/controls.frameSeconds))+2);std::vector<float> periods(periodCount);
            float nearest=0;for(float value:request.sourceMidi)if(value>0){nearest=value;break;}
            if(nearest<=0)throw std::runtime_error("MLD5 has no positive analysed source pitch");
            for(std::size_t i=0;i<periods.size();++i){const auto midi=controls.source(controls.mapped(i*controls.frameSeconds,true));if(midi>0)nearest=midi;const auto hz=440.*std::exp2((nearest-69.)/12.);if(hz<20||hz>4000)throw std::runtime_error("Analysed source F0 is outside MLD5 period support");periods[i]=float(principalRate/hz);}
            const melodyne_re::SampledFunctionView periodView{periods.data(),periods.size(),1./controls.frameSeconds};
            juce::AudioBuffer<float> work(request.input->getNumChannels(),workCount);work.clear();
            for(int channel=0;channel<pcm.getNumChannels();++channel)
            {
                const std::vector<float> component(pcm.getReadPointer(channel),pcm.getReadPointer(channel)+pcm.getNumSamples());
                melodyne_re::PeriodRendererParameters parameters;parameters.source_sample_rate=principalRate;parameters.output_sample_rate=renderRate;parameters.output_count=std::size_t(workCount);parameters.probe_ratio=q0;parameters.clip_output=false;
                std::vector<float> samples;
                if(controls.identity()&&allVoiced&&constantQ)
                {
                    // Highest recovered composition for admitted identity clocks:
                    // keep actual caller knots/count and source feedback; no
                    // guessed fixed four fixture anchors or post-hoc gain fit.
                    std::vector<double> knots;for(const auto& p:controls.map)knots.push_back(p.targetSeconds);
                    std::vector<float> playback(controlCount);for(std::size_t i=0;i<playback.size();++i)playback[i]=float(controls.ratio(i*controls.frameSeconds));
                    const melodyne_re::SampledFunctionView rates{playback.data(),playback.size(),1./controls.frameSeconds};
                    melodyne_re::SourceTimeRenderContext caller;caller.time.end_time=double(workCount)/renderRate;caller.time.output_block_count=workCount;caller.source_refresh_budget=std::min(139,workCount);caller.rate_external_budget=workCount;caller.admit_pre_startup_source_refresh=true;
                    melodyne_re::OutputGainRenderParameters gain;gain.signal_end=double(component.size());gain.pending_queue_empty=request.terminalComponentKnown&&request.pendingQueueEmpty;gain.last_component_zero_remainder=request.terminalComponentKnown&&request.pendingQueueEmpty&&request.allowZeroRemainder; // media EOF never fabricates queue state
                    auto result=melodyne_re::render_period_component_source_time_gain(component,periodView,rates,{knots.data(),knots.size()},parameters,caller,gain);
                    if(!result.ok)throw std::runtime_error(result.error);samples=std::move(result.samples);
                }
                else
                {
                    Mld5NativeControl::Parameters control;control.sourceRate=principalRate;control.outputRate=renderRate;control.duration=double(workCount)/renderRate;
                    control.sourceAt=[&](double t){return controls.mapped(t);};control.pitchRatioAt=[&](double t){return controls.ratio(t);};control.probeRatioAt=[&](double t){return controls.q(t);};control.processedFrames=!constantQ;
                    for(const auto& point:controls.map)if(point.targetSeconds>0&&point.targetSeconds<control.duration)control.targetBoundaries.push_back(point.targetSeconds);
                    for(std::size_t i=1;i<controlCount;++i){const auto t=i*controls.frameSeconds;if(t<control.duration)control.targetBoundaries.push_back(t);}
                    std::sort(control.targetBoundaries.begin(),control.targetBoundaries.end());control.targetBoundaries.erase(std::unique(control.targetBoundaries.begin(),control.targetBoundaries.end()),control.targetBoundaries.end());
                    Mld5NativeControl clock(std::move(control));auto result=melodyne_re::detail::render_period_component_controlled(component,periodView,parameters,&clock);
                    if(!result.ok)throw std::runtime_error(result.error);samples=std::move(result.samples);
                }
                if(samples.size()!=std::size_t(workCount))throw std::runtime_error("MLD5 recovered DSP returned an invalid output length");work.copyFrom(channel,0,samples.data(),workCount);
            }
            // Both lanes meet on the SAME output grid before the one final
            // rate conversion. Do not mix a twice-filtered wet lane with an
            // unaligned native-rate dry lane after conversion.
            for(int i=0;i<workCount;++i){const auto t=double(i)/renderRate;const auto gate=controls.gate(t);for(int c=0;c<work.getNumChannels();++c){const auto dry=readOriginal(pcm,c,controls.mapped(t)*principalRate);work.setSample(c,i,work.getSample(c,i)*gate+dry*(1.f-gate));}}
            rendered=resample(work,renderRate,request.sampleRate,count);
        }
        // An entirely UV source requires no periodic synthesis. Its original
        // audible mask remains independent of hidden carrier preparation.
        const bool exactRawIdentity=!anyVoiced&&controls.identity();
        for(int i=0;i<count;++i)
        {
            const double t=double(i)/request.sampleRate;const auto pos=t/controls.frameSeconds;
            const float gain=std::clamp(curve(request.noteGain,pos,1.f),0.f,4.f);
            for(int c=0;c<rendered.getNumChannels();++c)
            {
                const auto sample=anyVoiced?rendered.getSample(c,i):exactRawIdentity?(i<request.input->getNumSamples()?request.input->getSample(c,i):0.f):readOriginal(*request.input,c,controls.mapped(t)*request.sampleRate);
                const auto value=sample*gain;
                if(!std::isfinite(value))throw std::runtime_error("Non-finite recovered MLD5 output");rendered.setSample(c,i,value);
            }
        }
        return rendered;
    }
    catch(const std::exception& exception){if(error)*error=juce::String::fromUTF8(exception.what());return {};}
}
}
