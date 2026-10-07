#pragma once
// Native adaptation of openhachimi/hifisampler (Apache-2.0).
// Upstream revision and license are in third_party/hifisampler.
#include "UtauRenderer.h"
#include <regex>
#include <map>
#include <cmath>
#include <algorithm>

namespace hachi::backend
{
struct HifiFlagDefinition { const char* name; float minimum, maximum, initial; };
inline constexpr HifiFlagDefinition hifiFlagDefinitions[] {
    {"g", -600, 600, 0}, {"Hb", 0, 500, 100}, {"Hv", 0, 150, 100},
    {"Ht", -100, 100, 0}, {"HG", 0, 100, 0}, {"P", 0, 100, 100},
    {"t", -1200, 1200, 0}, {"A", -100, 100, 0}
};
inline const HifiFlagDefinition* hifiDefinition(const juce::String& name)
{
    for (const auto& d : hifiFlagDefinitions) if (name == d.name) return &d;
    return nullptr;
}
struct HifisamplerFlags
{
    std::map<juce::String, float> values;
    bool force = false, loop = false, normalize = false;
    juce::StringArray unsupported;
    float get(const juce::String& name) const
    {
        if (const auto i = values.find(name); i != values.end()) return i->second;
        if (const auto* d = hifiDefinition(name)) return d->initial;
        return 0;
    }
    void set(const juce::String& name, float value)
    {
        if (const auto* d = hifiDefinition(name); d && std::isfinite(value))
            values[name] = juce::jlimit(d->minimum, d->maximum, value);
    }
    static HifisamplerFlags parse(const juce::String& input)
    {
        HifisamplerFlags result;
        // Same integer syntax, slash removal and first-occurrence precedence
        // as util/parse_utau.py. Recognise unsupported tokens as complete names:
        // e.g. Mt is not t, HG is not G, and Hb is not B.
        const auto text = input.removeCharacters("/").toStdString();
        static const std::regex token("([A-Za-z]+)([+-]?[0-9]+)?");
        // Greedy letters must still allow adjacent valueless flags, e.g. GHe.
        static const std::regex supported("(HF|Mt|Rd|Mo|ME|Mr|MH|Mq|Mf|Mb|Ab|Md|Mn|NA|RG|MY|bh|Mm|K|V|u|b|fe|fl|fo|fv|fp|ve|vo|Hb|Hv|Ht|He|HG|g|t|A|B|G|P|S|p|R|D|C|Z)([+-]?[0-9]+)?");
        std::map<juce::String, bool> seen;
        std::size_t cursor = 0;
        while (cursor < text.size())
        {
            std::cmatch match;
            const auto* begin = text.c_str() + cursor;
            if (std::regex_search(begin, match, supported, std::regex_constants::match_continuous))
            {
                const juce::String key(match[1].str());
                if (!seen[key]) {
                    seen[key] = true;
                    if (key == "G") result.force = true;
                    else if (key == "He") result.loop = true;
                    else if (hifiDefinition(key)) {
                        if (key == "P") result.normalize = true;
                        if (match[2].matched) result.set(key, juce::String(match[2].str()).getFloatValue());
                    }
                    else result.unsupported.addIfNotAlreadyThere(key);
                }
                cursor += match.length();
            }
            else if (std::regex_search(begin, match, token, std::regex_constants::match_continuous))
            {
                result.unsupported.addIfNotAlreadyThere(juce::String(match[1].str()));
                cursor += match.length();
            }
            else ++cursor;
        }
        return result;
    }
};

inline HifisamplerFlags hifiFlagsAt(const UtauNoteRenderSpec& note,
    const std::array<double, 4>& regionLengths, int regionCount,
    double soundingTime, double soundStartOffset)
{
    auto region = 0;
    auto end = regionLengths[0];
    while (region + 1 < regionCount && soundingTime >= end)
        end += regionLengths[static_cast<std::size_t>(++region)];
    // Region text overrides the same flag, while the note/global text supplies
    // defaults for all remaining flags. An empty region inherits everything.
    auto result = HifisamplerFlags::parse((note.flagSplit && regionCount > 0
        ? note.regionFlags[static_cast<std::size_t>(region)] : juce::String{}) + note.flags);
    if (note.flagCurve)
        for (int pass = 0; pass < 2; ++pass)
        for (const auto& [rawName, points] : note.flagCurves)
        {
            if (rawName.startsWith("HIFI:") != (pass == 1)) continue;
            const auto name = rawName.startsWith("HIFI:") ? rawName.substring(5) : rawName;
            if (!hifiDefinition(name)) continue;
            // An explicit empty native curve disables only the legacy fallback.
            // Keep the WCSNDM data intact when resetting the HiFisampler lane.
            if (pass == 0 && std::any_of(note.flagCurves.begin(), note.flagCurves.end(),
                [&](const auto& curve) { return curve.first == "HIFI:" + name; })) continue;
            if (points.empty()) continue;
            const auto time = soundingTime + soundStartOffset;
            auto value = points.front().second;
            if (time >= points.back().first) value = points.back().second;
            else for (std::size_t i = 1; i < points.size(); ++i)
                if (time <= points[i].first) {
                    const auto u = juce::jlimit(0.0, 1.0, (time - points[i-1].first)
                        / std::max(1.0e-9, points[i].first - points[i-1].first));
                    value = points[i-1].second + u * (points[i].second - points[i-1].second);
                    break;
                }
            result.set(name, static_cast<float>(value));
            if (name == "P") result.normalize = true;
        }
    return result;
}
}
