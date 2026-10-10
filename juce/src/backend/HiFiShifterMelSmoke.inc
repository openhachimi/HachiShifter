// Numerical stretch contract plus real-model smoke, independent of GUI state.
bool runHiFiShifterMelSmoke(const juce::File& folder, const juce::File& directory)
{
    folder.createDirectory();
    bool ok = true;
    juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool passed) {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name);
        row->setProperty("passed", passed); checks.add(juce::var(row)); ok &= passed;
    };
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
    try {
        const MelData input { { 0, 10, 20, 30, 100, 110, 120, 130 }, 4 };
        const auto up = stretchHiFiShifterMel(input, 2, 1, 2, {{0,0},{2,1}});
        bool matches = up.frames == 8;
        for (int b=0; b<2 && matches; ++b)
            for (int i=0; i<8; ++i)
                matches &= std::abs(up.values[b*8+i] - (b*100.f+i*30.f/7.f)) < 1.e-4f;
        check("upstream_endpoint_linear_reference", matches);
        const auto down = stretchHiFiShifterMel(input, 2, 1, .5, {{0,0},{.5,1}});
        check("shorten_keeps_both_endpoints", down.frames==2 && down.values==std::vector<float>{0,30,100,130});
        const auto identity = stretchHiFiShifterMel(input,2,1,1,{{0,0},{1,1}});
        check("identity_mel_exact", identity.values==input.values);
        const auto single = stretchHiFiShifterMel(input,2,1,.125,{{0,0},{.125,1}});
        check("single_frame_uses_first", single.frames==1 && single.values==std::vector<float>{0,100});
        const auto held = stretchHiFiShifterMel({{7,9},1},2,1,8,{{0,0},{8,1}});
        check("single_source_frame_stretches", held.frames==8 && held.values.front()==7 && held.values.back()==9);
        const auto mapped = stretchHiFiShifterMel(input,2,1,2,{{0,0},{1,.25},{2,1}});
        check("nonuniform_timing_preserved", std::abs(mapped.values[3]-45.f/7.f)<1.e-4f
            && mapped.values[7]==30 && mapped.values[3]<up.values[3]);
        const auto seam = stretchHiFiShifterMel(input,2,1,2,{{0,0},{1,.25},{1,.75},{2,1}});
        check("source_jump_preserved", seam.values[3]<8 && seam.values[4]>22);
        const auto physical = spliceMelToTimeMap(input,2,8,.25,.25,{{0,0},{2,1}});
        check("distinct_from_engine_native_clock", std::abs(physical.values[6]-up.values[6])>1.f);
        std::vector<float> tail(4,1); alignHiFiShifterTail(tail,8,2);
        check("pad_tail_matches_upstream", tail==std::vector<float>{1,1,1,.5f,0,0,0,0});
        tail.assign(8,1); alignHiFiShifterTail(tail,4,2);
        check("trim_tail_matches_upstream", tail==std::vector<float>{1,1,1,.5f});
        tail.assign(4,1); alignHiFiShifterTail(tail,4,2);
        check("equal_duration_no_extra_tail_fade", tail==std::vector<float>(4,1));
        // Deliberately non-hop-aligned source duration exercises round(T_in/rate)
        // and the >1-hop shortfall produced by an 8x extension.
        constexpr int samples=12000, sr=44100;
        juce::AudioBuffer<float> source(1,samples);
        for (int i=0;i<samples;++i) {
            const auto t=double(i)/sr;
            source.setSample(0,i,float(.22*std::sin(juce::MathConstants<double>::twoPi*220*t)
                +.045*std::sin(juce::MathConstants<double>::twoPi*660*t)));
        }
        for (const auto ratio : {.5,1.,2.,8.}) {
            const auto count=int(samples*ratio);
            std::vector<float> pitch(std::size_t(std::ceil(count*1000./sr/5))+2,57.f);
            const auto result=NsfHifiganRenderer::render(source,sr,count,5,pitch,{},
                {{0,0},{double(count)/sr,double(samples)/sr}},directory,{},
                NsfHifiganStretchOrder::hifiShifterMel);
            const auto label="real_model_ratio_"+juce::String(ratio);
            bool valid=result.usedModel && result.error.isEmpty() && result.buffer.getNumSamples()==count;
            if (valid) {
                valid &= result.buffer.getRMSLevel(0,count/4,count/2)>1.e-5f;
                for(int i=0;i<count;++i) valid &= std::isfinite(result.buffer.getSample(0,i));
                juce::WavAudioFormat format;
                auto stream=folder.getChildFile(label+".wav").createOutputStream();
                std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.release(),sr,1,24,{},0));
                valid &= writer && writer->writeFromAudioSampleBuffer(result.buffer,0,count);
            }
            check(label.toRawUTF8(),valid);
        }
    } catch (const std::exception& e) {
        folder.getChildFile("error.txt").replaceWithText(e.what()); check("exception",false);
    }
#else
    check("onnx_build_required",false);
#endif
    auto* report=new juce::DynamicObject(); report->setProperty("passed",ok);report->setProperty("checks",checks);
    folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report),true));
    return ok;
}
