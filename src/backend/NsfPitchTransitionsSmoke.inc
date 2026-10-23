bool runNsfPitchTransitionsSmoke(const juce::File& folder,const juce::File& modelDirectory)
{
    folder.createDirectory(); bool ok=true; juce::Array<juce::var> checks;
    auto* report=new juce::DynamicObject();
    const auto check=[&](const char* name,bool pass) {
        auto* row=new juce::DynamicObject();row->setProperty("name",name);row->setProperty("passed",pass);
        checks.add(juce::var(row));ok &= pass;
    };
    const std::vector<float> step{220,220,440,440,440};
    auto disabled=step;conditionNsfPitchTransitions(disabled,false);
    check("off_keeps_every_authored_f0_frame",disabled==step);
    auto enabled=step;conditionNsfPitchTransitions(enabled,true);
    check("on_softens_octave_step",std::abs(enabled[1]-std::sqrt(220.f*440.f))<.001f
        && std::abs(enabled[2]-std::sqrt(220.f*440.f))<.001f && enabled.front()==220 && enabled.back()==440);
    const std::vector<float> glide{220,230,240,250,260};auto gentle=glide;
    conditionNsfPitchTransitions(gentle,true);check("on_retains_continuous_glissando",gentle==glide);
    std::vector<float> uv{220,220,0,0,440,440};auto originalUv=uv;
    conditionNsfPitchTransitions(uv,false);check("off_keeps_zero_f0_transitions",uv==originalUv);
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
    try {
        constexpr int sr=44100;constexpr double duration=.8;
        juce::AudioBuffer<float> source(1,int(sr*duration));juce::Random random(134);float previous=0;
        for(int i=0;i<source.getNumSamples();++i) {
            const auto t=double(i)/sr;const auto noise=random.nextFloat()*2-1;
            source.setSample(0,i,t<.6 ? float(.2*std::sin(juce::MathConstants<double>::twoPi*220*t))
                : (noise-previous)*.1f);previous=noise;
        }
        std::vector<float> target(162,69);for(std::size_t i=0;i<60;++i)target[i]=57;
        auto render=[&](bool smooth) {return NsfHifiganRenderer::render(source,sr,source.getNumSamples(),5,target,{},
            {{0,0},{duration,duration}},modelDirectory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel,
            false,{}, {},true,smooth);};
        auto on=render(true),off=render(false);
        check("real_model_renders_both_switch_states",on.usedModel && off.usedModel
            && on.buffer.getNumSamples()==source.getNumSamples() && off.buffer.getNumSamples()==source.getNumSamples());
        double difference=0,dryError=0;bool finite=on.usedModel && off.usedModel;
        if(finite)for(int i=0;i<source.getNumSamples();++i) {
            finite &= std::isfinite(on.buffer.getSample(0,i)) && std::isfinite(off.buffer.getSample(0,i));
            if(i>int(sr*.27) && i<int(sr*.34))difference+=std::abs(on.buffer.getSample(0,i)-off.buffer.getSample(0,i));
            if(i>int(sr*.66) && i<int(sr*.75))dryError=std::max(dryError,double(std::max(
                std::abs(on.buffer.getSample(0,i)-source.getSample(0,i)),
                std::abs(off.buffer.getSample(0,i)-source.getSample(0,i)))));
        }
        check("switch_reaches_decoder_audio",difference>1.e-4 && finite);
        check("off_does_not_disable_consonant_protection",on.protectedUnvoicedFrames>0 && off.protectedUnvoicedFrames>0 && dryError<2.e-5);
        report->setProperty("unvoiced_pcm_max_error",dryError);
        report->setProperty("jump_region_absolute_difference",difference);
    } catch(const std::exception& e) {report->setProperty("error",e.what());check("exception",false);}
#else
    check("onnx_build_required",false);
#endif
    report->setProperty("passed",ok);report->setProperty("checks",checks);
    folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report),true));return ok;
}
