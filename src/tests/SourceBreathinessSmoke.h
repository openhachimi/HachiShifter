#pragma once
#include "../backend/SourceBreathiness.h"
#include "../backend/SourceVoicing.h"

namespace hachi
{
inline bool runSourceBreathinessSmoke(const juce::File& folder, const juce::File& recording)
{
    using backend::SourceBreathiness; using backend::SourceVoicing;
    folder.createDirectory(); bool ok = true; juce::Array<juce::var> checks;
    const auto check = [&](const char* name, bool passed)
    {
        auto* row = new juce::DynamicObject(); row->setProperty("name", name); row->setProperty("passed", passed);
        checks.add(row); ok &= passed; std::cout << name << '=' << passed << std::endl;
    };
    constexpr int rate = 16000, count = rate * 2;
    std::vector<float> clean(count), light(count), strong(count), darkBreath(count), darkNoise(count), rich(count), vibrato(count), glide(count), quiet(count), silence(count), clipped(count);
    juce::Random random(140); double phase = 0, dark = 0;
    for (int i = 0; i < count; ++i)
    {
        const auto t = double(i) / rate, white = random.nextFloat() * 2.0 - 1;
        dark = .92 * dark + .08 * white;
        clean[std::size_t(i)] = float(.18 * std::sin(juce::MathConstants<double>::twoPi * 220 * t));
        light[std::size_t(i)] = clean[std::size_t(i)] + float(.05 * white);
        strong[std::size_t(i)] = clean[std::size_t(i)] + float(.16 * white);
        // Coloured low-frequency noise intentionally fails the old high-band gate.
        darkNoise[std::size_t(i)] = float(.1 * dark / .204);
        darkBreath[std::size_t(i)] = clean[std::size_t(i)] + darkNoise[std::size_t(i)];
        phase += juce::MathConstants<double>::twoPi * 220 * std::pow(2.0, .5 * std::sin(juce::MathConstants<double>::twoPi * 5 * t) / 12) / rate;
        vibrato[std::size_t(i)] = float(.18 * std::sin(phase) * (.8 + .2 * std::sin(juce::MathConstants<double>::twoPi * 7 * t)));
        glide[std::size_t(i)] = float(.18 * std::sin(juce::MathConstants<double>::twoPi * (220 * t + 100 * t * t)));
        for (int k = 1; k <= 23; ++k) rich[std::size_t(i)] += float(.15 / k * std::sin(juce::MathConstants<double>::twoPi * 90 * k * t));
        quiet[std::size_t(i)] = clean[std::size_t(i)] * .002f;
        clipped[std::size_t(i)] = juce::jlimit(-.06f, .06f, clean[std::size_t(i)]);
    }
    const auto markedShare = [](const SourceBreathiness& mask)
    {
        int marked = 0, total = 0;
        for (std::size_t i = 10; i + 10 < mask.marked.size(); ++i) { marked += mask.marked[i] > .5f; ++total; }
        return double(marked) / std::max(1, total);
    };
    const auto lightMask = SourceBreathiness::analyse(light, rate);
    const auto strongMask = SourceBreathiness::analyse(strong, rate);
    const auto darkMask = SourceBreathiness::analyse(darkBreath, rate);
    check("light_pitched_breath_is_marked", markedShare(lightMask) > .8);
    check("strong_pitched_breath_is_marked", markedShare(strongMask) > .8);
    check("coloured_low_frequency_breath_is_marked", markedShare(darkMask) > .8);
    check("coloured_frication_is_marked", markedShare(SourceBreathiness::analyse(darkNoise, rate)) > .8);
    check("clean_vowels_stay_solid", markedShare(SourceBreathiness::analyse(clean, rate)) < .01);
    check("rich_harmonics_stay_solid", markedShare(SourceBreathiness::analyse(rich, rate)) < .02);
    check("natural_vibrato_and_shimmer_stay_solid", markedShare(SourceBreathiness::analyse(vibrato, rate)) < .02);
    check("clean_pitch_glide_stays_solid", markedShare(SourceBreathiness::analyse(glide, rate)) < .02);
    check("quiet_periodic_vowel_stays_solid", markedShare(SourceBreathiness::analyse(quiet, rate)) < .01);
    check("silence_is_not_breath", markedShare(SourceBreathiness::analyse(silence, rate)) == 0);
    check("clipped_periodic_vowel_is_not_breath", markedShare(SourceBreathiness::analyse(clipped, rate)) < .02);
    const auto uv = SourceVoicing::analyse(light, rate);
    int falseUv = 0; for (std::size_t i = 10; i + 10 < uv.unvoiced.size(); ++i) falseUv += uv.unvoiced[i] > .5f;
    check("pitched_breath_retains_renderer_voiced_mask", falseUv == 0 && markedShare(lightMask) > .8);
    const auto normal = SourceBreathiness::analyse(clean, rate);
    auto mixed = clean;
    for (int i = int(.35 * rate); i < int(.8 * rate); ++i) mixed[std::size_t(i)] = strong[std::size_t(i)];
    for (int i = int(1.1 * rate); i < int(1.18 * rate); ++i) mixed[std::size_t(i)] = darkNoise[std::size_t(i)];
    const auto bounded = SourceBreathiness::analyse(mixed, rate);
    bool inside = true, outside = true;
    for (std::size_t i = 42; i <= 73; ++i) inside &= bounded.marked[i] > .5f;
    for (std::size_t i = 84; i <= 105; ++i) outside &= bounded.marked[i] == 0;
    check("breath_boundaries_preserve_intervening_vowel", inside && outside);
    check("short_aspiration_is_visible", bounded.marked[114] > .5f);
    bool cancelled = false; int visited = 0;
    try { SourceBreathiness::analyse(mixed, rate, [&] { if (++visited == 5) throw 140; }); }
    catch (int) { cancelled = true; }
    check("analysis_is_cancellable_between_frames", cancelled && visited == 5);
    check("empty_and_short_sources_safe", SourceBreathiness::analyse({}, rate).marked.empty()
        && SourceBreathiness::analyse(std::vector<float>(160, .1f), rate).marked.empty());
    double realNewSeconds = 0, realOldSeconds = 0, elapsed = 0;
    if (recording.existsAsFile())
    {
        juce::AudioFormatManager formats; formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(recording));
        check("real_source_readable", reader != nullptr && reader->lengthInSamples > 0);
        if (reader)
        {
            juce::AudioBuffer<float> buffer(int(reader->numChannels), int(reader->lengthInSamples));
            reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, true);
            std::vector<float> mono(buffer.getNumSamples(), 0);
            for (int c = 0; c < buffer.getNumChannels(); ++c) for (int i = 0; i < buffer.getNumSamples(); ++i)
                mono[std::size_t(i)] += buffer.getSample(c, i) / float(buffer.getNumChannels());
            const auto start = juce::Time::getMillisecondCounterHiRes(); const auto real = SourceBreathiness::analyse(mono, reader->sampleRate);
            elapsed = juce::Time::getMillisecondCounterHiRes() - start;
            const auto old = SourceVoicing::analyse(mono, reader->sampleRate);
            for (const auto v : real.marked) realNewSeconds += v * real.period;
            for (const auto v : old.unvoiced) realOldSeconds += v * old.period;
            check("real_recording_detects_more_than_clear_unvoiced", realNewSeconds > realOldSeconds + .25 && realNewSeconds < reader->lengthInSamples / reader->sampleRate * .7);
            check("real_recording_analysis_is_lightweight", elapsed < 2500);
            auto csv = folder.getChildFile("real-breath-evidence.csv").createOutputStream();
            if (csv)
            {
                csv->setPosition(0); csv->truncate(); csv->writeText("seconds,periodicity,noise_fraction,breath_display,render_unvoiced\n", false, false, nullptr);
                for (std::size_t i = 0; i < real.marked.size(); ++i)
                    csv->writeText(juce::String(i * real.period, 3) + "," + juce::String(real.periodicity[i], 6) + ","
                        + juce::String(real.noiseFraction[i], 6) + "," + juce::String(real.marked[i], 0) + ","
                        + juce::String(i < old.unvoiced.size() ? old.unvoiced[i] : 0, 0) + "\n", false, false, nullptr);
            }
            check("real_evidence_report_written", csv != nullptr);
        }
    }
    auto* report = new juce::DynamicObject(); report->setProperty("passed", ok); report->setProperty("checks", checks);
    report->setProperty("real_breath_seconds", realNewSeconds); report->setProperty("real_clear_unvoiced_seconds", realOldSeconds);
    report->setProperty("real_analysis_ms", elapsed); report->setProperty("new_models_added", false);
    std::cout << "real_breath_seconds=" << realNewSeconds << "\nreal_clear_unvoiced_seconds=" << realOldSeconds << "\nreal_analysis_ms=" << elapsed << std::endl;
    return folder.getChildFile("report.json").replaceWithText(juce::JSON::toString(juce::var(report), true)) && ok;
}
}
