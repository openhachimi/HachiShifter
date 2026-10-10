#pragma once

// Adapted from OpenUtau's ChineseCVVCPhonemizer (MIT).
// See third_party/openutau/NOTICE.md and LICENSE.txt.
#include "UtauRenderer.h"
#include "LegacyTextCodec.h"
#include "../Pinyin.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>

namespace hachi::backend
{
struct ChineseCvvcRules
{
    std::map<juce::String, juce::String> vowels, consonants, replacements;

    static ChineseCvvcRules parse(const juce::String& text)
    {
        ChineseCvvcRules rules;
        juce::String section;
        for (auto line : juce::StringArray::fromLines(text))
        {
            line = line.trim().trimCharactersAtStart(juce::String::charToString(0xfeff));
            if (line.isEmpty() || line.startsWithChar(';') || line.startsWithChar('#')) continue;
            if (line.startsWithChar('[')) { section = line.upToFirstOccurrenceOf("]", true, false).toUpperCase(); continue; }
            auto fields = juce::StringArray::fromTokens(line, "=", "");
            fields.trim();
            if (section == "[REPLACE]" && fields.size() >= 2)
                rules.replacements[fields[0]] = fields[1];
            else if ((section == "[VOWEL]" && fields.size() >= 3)
                || (section == "[CONSONANT]" && fields.size() >= 2))
            {
                auto& table = section == "[VOWEL]" ? rules.vowels : rules.consonants;
                auto sounds = juce::StringArray::fromTokens(fields[section == "[VOWEL]" ? 2 : 1], ",", "");
                sounds.trim(); sounds.removeEmptyStrings();
                for (const auto& sound : sounds) table[sound] = fields[0];
            }
        }
        return rules;
    }
    juce::String normalize(juce::String lyric) const
    {
        lyric = hachi::lyricInPinyin(lyric.trim());
        if (const auto it = replacements.find(lyric); it != replacements.end()) return it->second;
        return lyric;
    }
    static juce::String lookup(const std::map<juce::String, juce::String>& table, const juce::String& key)
    {
        const auto it = table.find(key);
        return it == table.end() ? key : it->second;
    }
};

struct ChineseCvvcPlan
{
    UtauRenderRequest request;
    std::vector<std::size_t> owners;
    std::vector<double> offsets;
    std::vector<std::vector<UtauPhonemeSpan>> spans;
    juce::String warning;
};

inline ChineseCvvcPlan planChineseCvvc(const UtauRenderRequest& source)
{
    ChineseCvvcPlan plan;
    plan.request = source;
    plan.request.chineseCvvc = false; // also prevents recursion through the shared render adapter
    plan.spans.resize(source.notes.size());
    juce::String error;
    const auto presamp = source.voicebankDirectory.getChildFile("presamp.ini");
    auto text = LegacyTextCodec::read(presamp, error);
    // ASCII oto.ini can be detected as UTF-8 even when the accompanying Chinese
    // presamp is GBK. Respect explicit encoding settings; otherwise retry GBK.
    if (!text && LegacyTextCodec::directoryCodePage(source.voicebankDirectory) == 0)
        text = LegacyTextCodec::read(presamp, error, 936);
    auto rules = text ? ChineseCvvcRules::parse(text->text) : ChineseCvvcRules{};
    if (rules.vowels.empty() || rules.consonants.empty())
    {
        plan.warning = "ZH CVVC: presamp.ini requires [VOWEL] and [CONSONANT]; using manual aliases";
        for (std::size_t i = 0; i < source.notes.size(); ++i) { plan.owners.push_back(i); plan.offsets.push_back(0); }
        return plan;
    }
    plan.request.notes.clear();
    struct Event { std::size_t owner; juce::String alias, kind; double start; float midi; bool base; };
    std::vector<Event> events;
    std::vector<std::size_t> order(source.notes.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return source.notes[a].startSeconds < source.notes[b].startSeconds; });
    std::vector<juce::String> lyrics;
    for (const auto& note : source.notes) lyrics.push_back(rules.normalize(note.alias));
    const auto adjacent = [&](std::size_t a, std::size_t b)
    {
        return !isRestLyric(source.notes[a].alias) && !isRestLyric(source.notes[b].alias)
            && source.notes[a].alias.isNotEmpty() && source.notes[b].alias.isNotEmpty()
            && std::abs(source.notes[a].startSeconds + source.notes[a].durationSeconds
                        - source.notes[b].startSeconds) < 0.002;
    };
    const auto mapped = [&](const juce::String& alias, float midi)
    { return UtauRenderer::mappedAliasTiming(source.voicebankDirectory, alias, midi); };
    for (std::size_t position = 0; position < order.size(); ++position)
    {
        const auto i = order[position];
        const auto& note = source.notes[i];
        if (source.cancelled && source.cancelled()) break;
        auto lyric = lyrics[i];
        const bool hasPrev = position > 0 && adjacent(order[position - 1], i);
        const bool hasNext = position + 1 < order.size() && adjacent(i, order[position + 1]);
        const auto prev = hasPrev ? order[position - 1] : i;
        const auto prevVowel = hasPrev ? ChineseCvvcRules::lookup(rules.vowels, lyrics[prev]) : juce::String("-");
        const auto add = [&](juce::String alias, const char* kind, double offset, float midi, bool base)
        { events.push_back({ i, std::move(alias), kind, note.startSeconds + offset, midi, base }); };
        if (isRestLyric(note.alias) || note.alias.trim().isEmpty())
        { add(note.alias, "rest", 0, note.midiNote, true); continue; }
        // Space-containing aliases and [alias] are explicit overrides. They are not re-phonemized.
        if (lyric.containsChar(' ') || (lyric.startsWithChar('[') && lyric.endsWithChar(']')))
        {
            if (lyric.startsWithChar('[') && lyric.endsWithChar(']')) lyric = lyric.substring(1, lyric.length() - 1).trim();
            const auto oto = mapped(lyric, note.midiNote);
            add(oto ? oto->resolvedAlias : lyric, "manual", 0, note.midiNote, true);
            continue;
        }
        if (lyric == "-" || lyric.equalsIgnoreCase("R"))
        {
            const auto oto = mapped(prevVowel + " R", note.midiNote);
            add(oto ? oto->resolvedAlias : juce::String("RR"), "release", 0, note.midiNote, true);
            continue;
        }
        const auto full = mapped(prevVowel + " " + lyric, note.midiNote);
        const auto cv = mapped(lyric, note.midiNote);
        if (!full && !cv)
        {
            add("RR", "missing", 0, note.midiNote, true);
            if (plan.warning.length() < 1000) plan.warning += "Missing CV: " + lyric + "; ";
            continue;
        }
        if (!full)
        {
            const auto vcMidi = hasPrev ? source.notes[prev].midiNote : note.midiNote;
            const auto vc = mapped(prevVowel + " " + ChineseCvvcRules::lookup(rules.consonants, lyric), vcMidi);
            if (vc)
            {
                const auto beat = 60.0 / std::max(1.0, note.bpm > 0 ? note.bpm : source.bpm);
                auto duration = cv->preutteranceSeconds;
                if (cv->overlapSeconds == 0 && duration < beat / 4) duration = std::min(beat / 4, duration * 2);
                if (cv->overlapSeconds < 0) duration = cv->preutteranceSeconds - cv->overlapSeconds;
                const auto scaled = std::max(beat / 16, duration * std::pow(2.0, 1.0 - juce::jlimit(0, 200, note.consonantVelocity) / 100.0));
                duration = hasPrev ? std::min(source.notes[prev].durationSeconds / 1.5, scaled)
                                   : std::min(std::max(beat / 16, duration * 2), scaled);
                if (duration > 1.e-5) add(vc->resolvedAlias, "VC", -duration, vcMidi, false);
            }
        }
        add(full ? full->resolvedAlias : cv->resolvedAlias, full ? "VCV" : "CV", 0, note.midiNote, true);
        if (!hasNext)
        {
            const auto release = mapped(ChineseCvvcRules::lookup(rules.vowels, lyric) + " R", note.midiNote);
            const auto beat = 60.0 / std::max(1.0, note.bpm > 0 ? note.bpm : source.bpm);
            const auto tail = std::min(note.durationSeconds / 6, beat / 8);
            if (release && tail > 1.e-5) add(release->resolvedAlias, "release", note.durationSeconds - tail, note.midiNote, false);
        }
    }
    std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
    std::vector<double> onset(source.notes.size(), 0);
    for (const auto& event : events)
    {
        const auto& parent = source.notes[event.owner];
        const auto timing = mapped(event.alias, event.midi);
        auto pre = timing ? timing->preutteranceSeconds * std::pow(2.0, 1.0 - juce::jlimit(0, 200, parent.consonantVelocity) / 100.0) : 0.0;
        if (event.base && parent.preutteranceOverrideEnabled) pre = parent.preutteranceSeconds;
        onset[event.owner] = std::min(onset[event.owner], event.start - parent.startSeconds - pre);
    }
    for (std::size_t e = 0; e < events.size(); ++e)
    {
        const auto& event = events[e];
        const auto& parent = source.notes[event.owner];
        auto piece = parent;
        const auto offset = event.start - parent.startSeconds;
        auto end = parent.startSeconds + parent.durationSeconds;
        if (e + 1 < events.size()) end = std::min(end, events[e + 1].start);
        if (end <= event.start + 1.e-6) continue;
        piece.alias = event.alias;
        piece.aliasIsResolved = event.kind != "manual" && event.kind != "rest";
        piece.startSeconds = event.start;
        piece.durationSeconds = end - event.start;
        piece.midiNote = event.midi;
        piece.preserveEnvelopeTiming = true;
        // Fit once in the parent's clock; do not repeat attack/release for every phoneme.
        piece.amplitudeEnvelope = UtauRenderer::fitAmplitudeEnvelope(parent.amplitudeEnvelope,
            -onset[event.owner], parent.durationSeconds);
        for (auto& point : piece.amplitudeEnvelope) point.timeSeconds -= offset;
        const auto midiShift = (parent.midiNote - piece.midiNote) * 100.0f;
        if (parent.timelinePitchCents)
            piece.timelinePitchCents = [curve = parent.timelinePitchCents, offset, midiShift](double t) { return curve(t + offset) + midiShift; };
        for (auto& point : piece.pitchCurve) { point.timeSeconds -= offset; point.cents += midiShift; }
        for (auto& curve : piece.flagCurves) for (auto& point : curve.second) point.first -= offset;
        if (!event.base)
        {
            piece.oto = {};
            piece.stpSeconds = 0;
            piece.preutteranceOverrideEnabled = piece.overlapOverrideEnabled = false;
        }
        const auto hasEarlier = std::any_of(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(e),
            [&](const auto& other) { return other.owner == event.owner; });
        const auto hasLater = std::any_of(events.begin() + static_cast<std::ptrdiff_t>(e + 1), events.end(),
            [&](const auto& other) { return other.owner == event.owner; });
        if (hasLater) piece.tailFadeMode = 0;
        if (hasEarlier) piece.tailFadeSettings.head.mode = 0;
        plan.spans[event.owner].push_back({ event.alias, event.kind, offset, end - parent.startSeconds });
        plan.owners.push_back(event.owner);
        plan.offsets.push_back(offset);
        plan.request.notes.push_back(std::move(piece));
    }
    return plan;
}

// Both renderers use this adapter. Generated pieces are folded back into their
// original note for waveform callbacks, so editing IDs and project notes never split.
template <typename Render>
UtauRenderResult renderChineseCvvc(const UtauRenderRequest& source, Render&& render)
{
    auto plan = planChineseCvvc(source);
    struct Captured { std::size_t owner; double start, rate; juce::AudioBuffer<float> audio; };
    std::vector<Captured> pieces;
    plan.request.notePhonemes = {};
    if (source.notePiece)
        plan.request.notePiece = [&](std::size_t index, const juce::AudioBuffer<float>& audio, double rate,
            double lead, const std::function<float(double)>& gain, const std::function<float(double)>&)
        {
            if (index >= plan.owners.size() || rate <= 0) return;
            Captured piece { plan.owners[index], plan.offsets[index] - lead, rate, audio };
            for (int s = 0; s < piece.audio.getNumSamples(); ++s)
                for (int c = 0; c < piece.audio.getNumChannels(); ++c)
                    piece.audio.setSample(c, s, piece.audio.getSample(c, s) * (gain ? gain(s / rate - lead) : 1.0f));
            pieces.push_back(std::move(piece));
        };
    auto result = render(plan.request);
    if (source.cancelled && source.cancelled()) return result;
    for (std::size_t owner = 0; owner < source.notes.size(); ++owner)
    {
        if (source.notePhonemes) source.notePhonemes(owner, plan.spans[owner]);
        double first = 0, last = 0, rate = 0;
        for (const auto& piece : pieces) if (piece.owner == owner)
        { first = std::min(first, piece.start); last = std::max(last, piece.start + piece.audio.getNumSamples() / piece.rate); rate = piece.rate; }
        if (!source.notePiece || rate <= 0 || last <= first) continue;
        juce::AudioBuffer<float> combined(2, static_cast<int>(std::ceil((last - first) * rate)));
        combined.clear();
        for (const auto& piece : pieces) if (piece.owner == owner)
        {
            const auto start = static_cast<int>(std::lround((piece.start - first) * rate));
            const auto count = std::min(combined.getNumSamples() - start, piece.audio.getNumSamples());
            for (int c = 0; c < 2; ++c) combined.addFrom(c, start, piece.audio, std::min(c, piece.audio.getNumChannels() - 1), 0, count);
        }
        source.notePiece(owner, combined, rate, -first, [](double) { return 1.0f; }, [](double) { return 1.0f; });
    }
    if (plan.warning.isNotEmpty()) result.warning += (result.warning.isEmpty() ? "" : "; ") + plan.warning;
    result.backend += "+zh-cvvc";
    return result;
}
}
