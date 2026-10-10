bool runNativeNoiseSmoke(const juce::File& folder, const juce::File& directory,
                         const juce::File& recording)
{
    folder.createDirectory(); bool ok=true;
    juce::Array<juce::var> checks;
    auto* report=new juce::DynamicObject();
    const auto check=[&](const char* name,bool pass) {
        auto* row=new juce::DynamicObject(); row->setProperty("name",name);
        row->setProperty("passed",pass);checks.add(juce::var(row));ok &= pass;
    };
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
    try {
        constexpr int sr=44100;
        juce::AudioBuffer<float> source(1,sr*2); juce::Random random(131);
        float previous=0;
        for(int i=0;i<source.getNumSamples();++i) {
            const auto t=double(i)/sr;
            const auto white=random.nextFloat()*2-1;
            const auto friction=(white-previous)*.12f; previous=white;
            auto value=0.f;
            if(t<.25 || (t>=.65 && t<.85)) value=friction;
            else if(t<1.2) value=float(.2*std::sin(juce::MathConstants<double>::twoPi*220*t)
                +.04*std::sin(juce::MathConstants<double>::twoPi*660*t))+(t>=.85 ? white*.09f : 0.f);
            else if(t<1.5) value=float(.04*std::sin(juce::MathConstants<double>::twoPi*85*t));
            else if(t<1.8) value=float(.12*std::sin(juce::MathConstants<double>::twoPi*850*t));
            source.setSample(0,i,value);
        }
        std::vector<float> mono(source.getReadPointer(0),source.getReadPointer(0)+source.getNumSamples());
        const auto uv=SourceVoicing::analyse(mono,sr);
        check("fricatives_are_unvoiced",uv.at(.1)==1 && uv.at(.75)==1);
        check("vowels_stay_voiced",uv.at(.4)==0);
        check("breathy_vowel_keeps_pitch",uv.at(1.0)==0);
        check("quiet_low_and_high_vowels_keep_pitch",uv.at(1.35)==0 && uv.at(1.65)==0);
        check("silence_is_unvoiced",uv.at(1.9)==1);
        const auto write=[&](const juce::File& path,const juce::AudioBuffer<float>& audio,double rate) {
            path.deleteFile(); juce::WavAudioFormat format; auto stream=path.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),rate,
                unsigned(audio.getNumChannels()),24,{},0));
            return writer && writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());
        };
        const auto fixture=folder.getChildFile("fricative-vowel.wav");
        check("fixture_written",write(fixture,source,sr));
        const auto fcpeModel=directory.getParentDirectory().getChildFile("fcpe/fcpe.onnx");
        juce::String error;
        const auto frames=FcpeAnalyzer::analyse(fixture,fcpeModel,{InferenceBackend::cpu,-1,2},error);
        bool noiseClean=!frames.empty(), vowelFound=false;
        for(const auto& frame:frames) {
            if((frame.timeSeconds>.05 && frame.timeSeconds<.18)
                || (frame.timeSeconds>.7 && frame.timeSeconds<.8)) noiseClean &= !frame.voiced;
            if(frame.timeSeconds>.35 && frame.timeSeconds<.5 && frame.voiced && std::abs(frame.midi-57)<.6) vowelFound=true;
        }
        check("fcpe_noise_mask_independent_of_pitch_bins",noiseClean && error.isEmpty());
        check("fcpe_retains_vowel_f0",vowelFound);
        const auto files=modelFiles(directory);check("models_available",files.has_value());
        if(!files) throw std::runtime_error("missing NSF models");
        auto separated=hifiSeparate(mono,*files,{InferenceBackend::cpu,-1,2},false);
        auto cached=hifiSeparate(mono,*files,{InferenceBackend::cpu,-1,2},false);
        check("separation_reused_after_pitch_edit",cached.cacheHit && cached.harmonic==separated.harmonic);
        auto first=NsfHifiganRenderer::render(source,sr,sr*2,5,std::vector<float>(402,69),{},
            {{0,0},{2,2}},directory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel,
            false,{}, {},true);
        check("native_uses_harmonic_noise_separation",first.usedModel && first.usedHarmonicNoise);
        check("forced_f0_removed_from_friction",first.protectedUnvoicedFrames>20);
        if(!first.usedModel) throw std::runtime_error(first.error.toStdString());
        double errorMax=0;
        for(int i=int(sr*.05);i<int(sr*.18);++i)
            errorMax=std::max(errorMax,double(std::abs(first.buffer.getSample(0,i)-source.getSample(0,i))));
        check("unvoiced_pcm_survives_octave_shift",errorMax<2.e-5);
        const auto powerAt=[&](double hz) {
            double a=0,b=0;
            for(int i=int(sr*.33);i<int(sr*.57);++i) {
                const auto phase=juce::MathConstants<double>::twoPi*hz*i/sr;
                a+=first.buffer.getSample(0,i)*std::cos(phase);b+=first.buffer.getSample(0,i)*std::sin(phase);
            }
            return a*a+b*b;
        };
        check("harmonic_vowel_transposes",powerAt(440)>powerAt(220)*2);
        check("protected_fixture_written",write(folder.getChildFile("fricative-vowel-shifted.wav"),first.buffer,sr));
        auto stretched=NsfHifiganRenderer::render(source,sr,sr*3,5,std::vector<float>(602,64),{},
            {{0,0},{1,.5},{3,2}},directory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel,
            false,{}, {},true);
        check("noise_follows_piecewise_time_map",stretched.usedModel && stretched.buffer.getNumSamples()==sr*3);
        double stretchError=0;
        if(stretched.usedModel) for(int i=int(sr*.1);i<int(sr*.3);++i) {
            const auto pos=double(i)*.5;const auto j=int(pos);
            const auto expected=source.getSample(0,j)+(source.getSample(0,j+1)-source.getSample(0,j))*float(pos-j);
            stretchError=std::max(stretchError,double(std::abs(stretched.buffer.getSample(0,i)-expected)));
        }
        check("noise_not_pitch_shifted_when_stretched",stretched.usedModel && stretchError<2.e-5);
        // Reproduce the user's recording with aggressively flattened frame
        // targets, including intentionally wrong targets on unvoiced material.
        juce::AudioFormatManager formats;formats.registerBasicFormats();
        auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(recording));
        if(!reader) throw std::runtime_error("missing requested recording");
        const auto rate=reader->sampleRate;
        juce::AudioBuffer<float> real(int(reader->numChannels),int(reader->lengthInSamples));
        reader->read(&real,0,real.getNumSamples(),0,true,true);
        const auto duration=real.getNumSamples()/rate;
        auto realMono=stableMonoInput(real);
        auto modelMono=resample(realMono.data(),real.getNumSamples(),int(std::llround(rate)),44100);
        const auto realUv=SourceVoicing::analyse(modelMono,44100);
        auto realFrames=FcpeAnalyzer::analyse(recording,fcpeModel,{InferenceBackend::cpu,-1,2},error);
        check("recording_pitch_analysis",!realFrames.empty() && error.isEmpty());
        std::vector<float> target(std::size_t(std::ceil(duration/.005))+1,64);
        std::size_t cursor=0;
        for(std::size_t i=0;i<target.size() && !realFrames.empty();++i) {
            while(cursor+1<realFrames.size() && realFrames[cursor+1].timeSeconds<=i*.005) ++cursor;
            if(realFrames[cursor].voiced) target[i]=std::round(realFrames[cursor].midi)+3;
        }
        const auto started=juce::Time::getMillisecondCounterHiRes();
        auto protectedReal=NsfHifiganRenderer::render(real,rate,real.getNumSamples(),5,target,{},
            {{0,0},{duration,duration}},directory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel,
            false,{}, {},true);
        report->setProperty("real_render_seconds",(juce::Time::getMillisecondCounterHiRes()-started)/1000);
        check("recording_separated_render",protectedReal.usedModel && protectedReal.usedHarmonicNoise);
        if(!protectedReal.usedModel) throw std::runtime_error(protectedReal.error.toStdString());
        double realError=0;int protectedSamples=0;
        for(int i=int(rate*.03);i<real.getNumSamples()-int(rate*.03);++i)
            if(realUv.at(i/rate)==1.f) {
                ++protectedSamples;
                for(int c=0;c<real.getNumChannels();++c)
                    realError=std::max(realError,double(std::abs(real.getSample(c,i)-protectedReal.buffer.getSample(c,i))));
            }
        report->setProperty("protected_unvoiced_seconds",protectedSamples/rate);
        report->setProperty("protected_pcm_max_error",realError);
        check("recording_contains_protected_consonants",protectedSamples>rate*.1);
        check("recording_unvoiced_pcm_preserved",realError<2.e-5);
        check("recording_preview_written",write(folder.getChildFile("real-protected.wav"),protectedReal.buffer,rate));
        // Local-edit benchmark on the user's real recording, through the same
        // HiFiShifter Mel and H/N protection path used by the native editor.
        report->setProperty("real_cold_inferred_chunks", protectedReal.inferredChunks);
        auto localTarget = target;
        auto editFrame = localTarget.size() / 2;
        while (editFrame + 60 < localTarget.size() && realUv.at(editFrame * .005) >= .999f) ++editFrame;
        for (auto i = editFrame; i < std::min(localTarget.size(), editFrame + 60); ++i) localTarget[i] += 2;
        const auto localStarted = juce::Time::getMillisecondCounterHiRes();
        auto localReal = NsfHifiganRenderer::render(real,rate,real.getNumSamples(),5,localTarget,{},
            {{0,0},{duration,duration}},directory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel,
            false,{}, {},true);
        report->setProperty("real_local_edit_seconds", (juce::Time::getMillisecondCounterHiRes()-localStarted)/1000);
        report->setProperty("real_local_inferred_chunks", localReal.inferredChunks);
        report->setProperty("real_local_reused_chunks", localReal.reusedChunks);
        check("recording_local_edit_reuses_neural_chunks", localReal.usedModel
            && localReal.inferredChunks > 0 && localReal.inferredChunks < protectedReal.inferredChunks && localReal.reusedChunks > 0);
        double localNoiseError = 0;
        if (localReal.usedModel) for(int i=int(rate*.03);i<real.getNumSamples()-int(rate*.03);++i)
            if(realUv.at(i/rate)==1.f) for(int c=0;c<real.getNumChannels();++c)
                localNoiseError=std::max(localNoiseError,double(std::abs(real.getSample(c,i)-localReal.buffer.getSample(c,i))));
        check("recording_cached_edit_preserves_unvoiced_pcm", localReal.usedModel && localNoiseError < 2.e-5);
        auto old=NsfHifiganRenderer::render(real,rate,real.getNumSamples(),5,target,{},
            {{0,0},{duration,duration}},directory,{InferenceBackend::cpu,-1,2},NsfHifiganStretchOrder::hifiShifterMel);
        check("old_pipeline_comparison_written",old.usedModel && write(folder.getChildFile("real-pitch-only.wav"),old.buffer,rate));
    } catch(const std::exception& e) {
        report->setProperty("error",e.what());check("exception",false);
    }
#else
    check("onnx_build_required",false);
#endif
    report->setProperty("passed",ok);report->setProperty("checks",checks);
    folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report),true));
    return ok;
}
