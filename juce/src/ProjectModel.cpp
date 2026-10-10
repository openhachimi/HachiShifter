#include "backend/MixedEnvelopeIO.h"
#include "NativeAudioTrim.h"
#include "NativeNoteJoin.h"
#include "NativeAudioOverlap.h"
#include "NativeAudioLink.h"
#include "NativeAudioDisconnect.h"
#include "NativeUnpitchedRegions.h"
#include "DiffSingerParameterCurves.h"
#include "DiffSingerPitchHandles.h"
#include "ProjectModel.h"
#include "backend/UstExchange.h"
#include "ProjectFileIO.h"
#include "HamoodProject.h"
#include "TrackGainEnvelope.h"
#include "ClipParts.h"
#include "Pinyin.h"
#include "SampleSettings.h"
#include "backend/UstImporter.h"
#include "backend/UtauRenderer.h"
#include "backend/AmplitudeEnvelopeCurve.h"
#include "backend/NsfHifiganRenderer.h"
#include "backend/AnalysisService.h"
#include "NativeSourceTimeMap.h"
#include "NativeNoteTiming.h"
#include "NativePitchIdentity.h"
#include "NativeAudioClipboard.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <optional>

namespace hachi
{
PitchAlgorithm defaultPitchAlgorithm(const juce::File& modelDirectory)
{
    return backend::NsfHifiganRenderer::modelAvailable(modelDirectory)
        ? PitchAlgorithm::nsfHifigan : PitchAlgorithm::llsm2;
}
juce::String nativeSegmentRoleName(NativeSegmentRole role)
{
    switch (role)
    {
        case NativeSegmentRole::consonant: return "consonant";
        case NativeSegmentRole::vowel: return "vowel";
        case NativeSegmentRole::transition: return "transition";
        case NativeSegmentRole::silence: return "silence";
        case NativeSegmentRole::breath: return "breath";
        case NativeSegmentRole::noise: return "noise";
        case NativeSegmentRole::ending: return "ending";
        case NativeSegmentRole::unknown: break;
    }
    return "unknown";
}

NativeSegmentRole parseNativeSegmentRole(const juce::String& value)
{
    const auto role = value.trim().toLowerCase();
    if (role == "consonant") return NativeSegmentRole::consonant;
    if (role == "vowel") return NativeSegmentRole::vowel;
    if (role == "transition") return NativeSegmentRole::transition;
    if (role == "silence") return NativeSegmentRole::silence;
    if (role == "breath") return NativeSegmentRole::breath;
    if (role == "noise") return NativeSegmentRole::noise;
    if (role == "ending") return NativeSegmentRole::ending;
    return NativeSegmentRole::unknown;
}

float renderedPitchCents(const NoteData& note, const PitchPoint& point)
{
    if (point.hasManualTarget) return point.manualTargetCents;

    // Melodyne stores an exact zero for notes flattened with the pitch
    // modulation tool.  Some analysed source points do not contain a separate
    // pitchWithoutVibrato curve (it is byte-for-byte equal to pitchCent), so
    // the generic drift/modulation decomposition would otherwise leave the
    // original contour untouched.  Treat the saved zero as the authoritative
    // flat-note edit for both display and rendering.
    if (note.modulation <= 1.0e-4f) return 0.0f;

    return note.drift * point.withoutVibratoCents
        + note.modulation * (point.relativeCents - point.withoutVibratoCents);
}

float shapedSegmentProgress(PitchCurveShape shape, float u,
                            float bezierX1, float bezierY1,
                            float bezierX2, float bezierY2)
{
    u = juce::jlimit(0.0f, 1.0f, u);
    switch (shape)
    {
        case PitchCurveShape::smooth: return u * u * (3.0f - 2.0f * u);
        case PitchCurveShape::easeIn: return u * u;
        case PitchCurveShape::easeOut: return 1.0f - (1.0f - u) * (1.0f - u);
        case PitchCurveShape::customBezier: break;
        // A contour-wide shape has no per-segment answer: whoever owns a whole
        // contour handles that one, and everything else is a straight line.
        case PitchCurveShape::natural:
        case PitchCurveShape::linear:
        default: return u;
    }
    // Invert the Bezier X component to turn the normalised timeline U into the
    // curve parameter T, then evaluate Y.  Newton iteration is fast for
    // ordinary handles; bisection is the fallback around flat derivatives.
    const auto x1 = juce::jlimit(0.0f, 1.0f, bezierX1);
    const auto x2 = juce::jlimit(0.0f, 1.0f, bezierX2);
    const auto cubic = [](float t, float first, float second)
    {
        const auto inverse = 1.0f - t;
        return 3.0f * inverse * inverse * t * first
            + 3.0f * inverse * t * t * second + t * t * t;
    };
    const auto derivative = [](float t, float first, float second)
    {
        const auto inverse = 1.0f - t;
        return 3.0f * inverse * inverse * first
            + 6.0f * inverse * t * (second - first)
            + 3.0f * t * t * (1.0f - second);
    };
    auto parameter = u;
    for (auto iteration = 0; iteration < 6; ++iteration)
    {
        const auto slope = derivative(parameter, x1, x2);
        if (std::abs(slope) < 1.0e-5f) break;
        const auto candidate = parameter - (cubic(parameter, x1, x2) - u) / slope;
        if (candidate < 0.0f || candidate > 1.0f) break;
        parameter = candidate;
    }
    auto lower = 0.0f;
    auto upper = 1.0f;
    for (auto iteration = 0; iteration < 18; ++iteration)
    {
        if (cubic(parameter, x1, x2) < u) lower = parameter;
        else upper = parameter;
        parameter = (lower + upper) * 0.5f;
    }
    return cubic(parameter, bezierY1, bezierY2);
}

juce::String utauModeLabel(UtauMode mode)
{
    return mode == UtauMode::jie ? juce::String::fromUTF8("界•UTAU")
         : mode == UtauMode::mou ? juce::String::fromUTF8("谋•UTAU")
                                 : juce::String("UTAU");
}

juce::String utauModeKey(UtauMode mode)
{
    // "utau4" is what 界 has always been called in project files and over
    // MCP, so it stays exactly that; only the new mode needs a new word.
    return mode == UtauMode::jie ? juce::String("utau4")
         : mode == UtauMode::mou ? juce::String("utaumou")
                                 : juce::String("utau");
}

int utauModePickerItem(UtauMode mode)
{
    return mode == UtauMode::jie ? 8 : mode == UtauMode::mou ? 9 : 7;
}

std::optional<UtauMode> utauModeForPickerItem(int itemId)
{
    switch (itemId)
    {
        case 7: return UtauMode::classic;
        case 8: return UtauMode::jie;
        case 9: return UtauMode::mou;
        default: return std::nullopt;
    }
}

float scaledEnvelopeGainDb(float gainDb, double factor)
{
    // Silence is silence however it is scaled, and the renderer reads anything
    // at or below -60 dB as nothing at all.
    if (gainDb <= -59.9f || factor <= 1.0e-9) return -60.0f;
    const auto level = std::pow(10.0, gainDb / 20.0) * factor;
    if (level <= 1.0e-4) return -60.0f;
    // The same ceiling the UST importer writes envelopes against: +12 dB is
    // 400%, which is as far as a base of 200 can take a point already at 200%.
    return juce::jlimit(-60.0f, 12.0f, static_cast<float>(20.0 * std::log10(level)));
}

std::vector<AmplitudeEnvelopePoint> scaledAmplitudeEnvelope(
    const std::vector<AmplitudeEnvelopePoint>& points, float basePercent)
{
    auto scaled = points;
    const auto factor = juce::jlimit(0.0f, 200.0f, basePercent) / 100.0;
    if (std::abs(factor - 1.0) < 1.0e-9) return scaled;
    for (auto& point : scaled) point.gainDb = scaledEnvelopeGainDb(point.gainDb, factor);
    return scaled;
}

std::vector<AmplitudeEnvelopePoint> unscaledAmplitudeEnvelope(
    const std::vector<AmplitudeEnvelopePoint>& points, float basePercent)
{
    auto plain = points;
    const auto factor = juce::jlimit(0.0f, 200.0f, basePercent) / 100.0;
    if (std::abs(factor - 1.0) < 1.0e-9 || factor <= 1.0e-9) return plain;
    for (auto& point : plain) point.gainDb = scaledEnvelopeGainDb(point.gainDb, 1.0 / factor);
    return plain;
}

UtauMode parseUtauMode(const juce::String& text)
{
    const auto value = text.trim().toLowerCase();
    if (value == "utau4" || value == "jie") return UtauMode::jie;
    if (value == "utaumou" || value == "mou") return UtauMode::mou;
    return UtauMode::classic;
}

const std::vector<FlagCurveKind>& flagCurveKinds()
{
    // Ordered as they are offered: the formant shift first, since it is what a
    // curve is usually wanted for, then the rest of the timbre controls.
    static const std::vector<FlagCurveKind> kinds {
        { "g",  "共振峰平移", -50.0f,  50.0f },
        { "Mt", "张力",      -100.0f, 100.0f },
        { "Rd", "声门 Rd",    -100.0f, 100.0f },
        { "Mo", "开口度",     -100.0f, 100.0f },
        { "ME", "共振峰强调", -100.0f, 100.0f },
        { "Mr", "歌手共振峰", -100.0f, 100.0f },
        { "MH", "高频滚降",   -100.0f, 100.0f },
        { "Mq", "高次谐波滚降", 0.0f,  100.0f },
        { "Mf", "共振峰调谐",   0.0f,  100.0f },
        { "Mb", "元音气声",   -100.0f, 100.0f },
        { "Ab", "全帧气声",   -100.0f, 100.0f },
        { "Md", "干燥度",     -100.0f, 100.0f },
        { "Mn", "噪声平滑",   -100.0f, 100.0f },
        { "NA", "鼻音度",     -100.0f, 100.0f },
        { "RG", "自动混声",   -100.0f, 100.0f },
        { "b",  "清辅音噪声",  -20.0f, 100.0f, true },
        { "bh", "辅音区谐波",  -20.0f, 100.0f, true },
        // 嘶吼 is not a frame parameter like the rest of these: the engine runs
        // it on the finished mix and reads this curve for its depth, sample by
        // sample.  Drawn in the same lane all the same -- one way to shape a
        // flag, whichever end of the engine reads it.
        { "MY", "嘶吼",          0.0f, 100.0f }
    };
    return kinds;
}

const std::vector<FlagCurveKind>& hifisamplerFlagCurveKinds()
{
    // Namespace avoids changing the ranges/units of saved WCSNDM curves.
    static const std::vector<FlagCurveKind> kinds {
        {"HIFI:g", "共振峰 / 性别", -600, 600},
        {"HIFI:Hb", "气声 / 噪声", 0, 500, false, 100},
        {"HIFI:Hv", "谐波 / 实声", 0, 150, false, 100},
        {"HIFI:Ht", "张力", -100, 100},
        {"HIFI:HG", "嘶吼 / 沙哑", 0, 100},
        {"HIFI:P", "响度归一化强度", 0, 100, false, 100},
        {"HIFI:t", "音高偏移（音分）", -1200, 1200},
        {"HIFI:A", "随音高变化调制振幅", -100, 100}
    };
    return kinds;
}

const std::vector<FlagCurveKind>& diffSingerFlagCurveKinds()
{
    static const std::vector<FlagCurveKind> kinds {
        { "DS:DYN", "响度（0.1 dB）", -240, 120 },
        { "DS:GENC", "共振峰 / 性别", -100, 100 },
        { "DS:VELC", "发音速度", 0, 200, false, 100 },
        { "DS:ENE", "能量偏移", -100, 100 },
        { "DS:BREC", "气声偏移", -100, 100 },
        { "DS:TENC", "张力偏移", -100, 100 },
        { "DS:VOIC", "实声偏移（100 为零偏移）", 0, 100, false, 100 },
        { "DS:PEXP", "音高表现（生成 pitch 时）", 0, 100, false, 100 },
        { "DS:SHFC", "音区偏移（音分）", -1200, 1200 }
    };
    return kinds;
}

const std::vector<FlagCurveKind>& diffSingerParameterKinds()
{
    static const std::vector<FlagCurveKind> kinds {
        {"DS:ABS:ENE", "能量实参（dB）", -96, 0, false, -40},
        {"DS:ABS:BREC", "气声实参（dB）", -96, 0, false, -40},
        {"DS:ABS:TENC", "张力实参（模型单位）", -10, 10},
        {"DS:ABS:VOIC", "实声量实参（dB）", -96, 0, false, -40}
    };
    return kinds;
}

const FlagCurveKind& flagCurveKindFor(const juce::String& flag)
{
    for (const auto& kind : hifisamplerFlagCurveKinds())
        if (flag == kind.flag) return kind;
    for (const auto& kind : diffSingerParameterKinds())
        if (isDiffSingerParameter(flag) && flag.fromLastOccurrenceOf(":", false, false)
            == juce::String(kind.flag).fromLastOccurrenceOf(":", false, false)) return kind;
    for (const auto& kind : diffSingerFlagCurveKinds())
        if (flag == kind.flag) return kind;
    static const FlagCurveKind colour { "DS:CLR:", "音色混合权重（%）", 0, 100 };
    if (flag.startsWith("DS:CLR:")) return colour;
    for (const auto& kind : flagCurveKinds())
        if (flag == kind.flag) return kind;
    return flagCurveKinds().front();
}

std::vector<FlagCurvePoint> flagCurvePointsFor(const NoteData& note,
                                               const juce::String& flag)
{
    for (const auto& curve : note.utauFlagCurves)
        if (curve.flag == flag) return curve.points;
    if (flag.startsWith("HIFI:")) return flagCurvePointsFor(note, flag.substring(5));
    if (flag.startsWith("DS:ABS:")) return flagCurvePointsFor(note, "DS:REF:"+flag.substring(7));
    if (flag.startsWith("DS:REF:")) return flagCurvePointsFor(note, "DS:AUTO:"+flag.substring(7));
    return {};
}

std::vector<std::pair<juce::String, std::vector<std::pair<double, double>>>>
sampleDiffSingerFlagCurves(const NoteData& note)
{
    std::vector<std::pair<juce::String, std::vector<std::pair<double, double>>>> result;
    auto all = note.utauFlagCurves;
    for (const auto& kind : diffSingerParameterKinds()) {
        // Automatic snapshots are display/persistence metadata, not frozen synthesis inputs.
        const auto code = juce::String(kind.flag).substring(7);
        const auto explicitBase = std::any_of(note.utauFlagCurves.begin(), note.utauFlagCurves.end(),
            [&](const auto& c) { return !c.points.empty() && (c.flag == kind.flag || c.flag == "DS:REF:"+code); });
        const auto base = explicitBase ? flagCurvePointsFor(note, kind.flag) : std::vector<FlagCurvePoint>{};
        if (!base.empty()) all.push_back({kind.flag, base});
    }
    std::set<juce::String> emitted;
    for (const auto& curve : all)
    {
        if (!curve.flag.startsWith("DS:") || curve.points.empty()) continue;
        if (curve.flag.startsWith("DS:REF:") || curve.flag.startsWith("DS:PTS:") || curve.flag.startsWith("DS:AUTO:")) continue;
        if (!note.utauFlagCurveEnabled && !curve.flag.startsWith("DS:ABS:")) continue;
        if (!emitted.insert(curve.flag).second) continue;
        std::vector<std::pair<double, double>> sampled;
        for (std::size_t i = 0; i < curve.points.size(); ++i)
        {
            const auto& point = curve.points[i];
            if (i > 0 && point.shape != PitchCurveShape::linear)
            {
                const auto from = curve.points[i-1].timeSeconds;
                const auto steps = juce::jlimit(2, 6500, static_cast<int>(std::ceil((point.timeSeconds-from)/.005)));
                for (int step = 1; step < steps; ++step)
                {
                    const auto time = from + (point.timeSeconds-from)*step/steps;
                    sampled.emplace_back(time, flagCurveValueAt(curve.points, time));
                }
            }
            sampled.emplace_back(point.timeSeconds, point.value);
        }
        result.emplace_back(curve.flag, std::move(sampled));
    }
    return result;
}

float flagCurveValueAt(const std::vector<FlagCurvePoint>& points, double timeSeconds)
{
    if (points.empty()) return 0.0f;
    if (timeSeconds <= points.front().timeSeconds) return points.front().value;
    if (timeSeconds >= points.back().timeSeconds) return points.back().value;
    const auto right = std::upper_bound(points.begin(), points.end(), timeSeconds,
        [](double value, const FlagCurvePoint& point)
        {
            return value < point.timeSeconds;
        });
    if (right == points.begin()) return points.front().value;
    if (right == points.end()) return points.back().value;
    const auto& next = *right;
    const auto& left = *std::prev(right);
    const auto span = next.timeSeconds - left.timeSeconds;
    if (span <= 1.0e-9) return next.value;
    const auto u = static_cast<float>((timeSeconds - left.timeSeconds) / span);
    // The shape belongs to the segment arriving at a point, as on the pitch
    // line, so the first point never has its own shape consulted.
    const auto shaped = shapedSegmentProgress(next.shape, u, next.bezierX1,
                                              next.bezierY1, next.bezierX2,
                                              next.bezierY2);
    return left.value + (next.value - left.value) * shaped;
}

float diffSingerPitchOffsetAt(const NoteData& note, double time)
{
    return note.diffSingerPitchOffset.empty() ? 0.0f : evaluatePitchCurve(note.diffSingerPitchOffset, time);
}

std::vector<PitchCurveEditPoint> ownPitchPoints(const NoteData& note)
{
    if (!note.pitchControlPoints.empty()) return note.pitchControlPoints;
    // The contour as the renderer reads it: every point, joined straight.
    std::vector<PitchCurveEditPoint> points;
    points.reserve(note.contour.size());
    for (const auto& point : note.contour)
    {
        PitchCurveEditPoint own { point.timeSeconds,
                                  note.midiNote + renderedPitchCents(note, point) / 100.0f };
        own.shape = PitchCurveShape::linear;
        points.push_back(own);
    }
    if (points.empty())
        points = { { 0.0, note.midiNote }, { note.durationSeconds, note.midiNote } };
    std::stable_sort(points.begin(), points.end(), [](const auto& left, const auto& right)
    {
        return left.timeSeconds < right.timeSeconds;
    });
    return points;
}

float SharedPitchLine::midiAt(double absoluteSeconds) const
{
    if (pieces.empty()) return 60.0f;
    auto piece = pieces.begin();
    for (auto next = std::next(pieces.begin()); next != pieces.end(); ++next)
    {
        if (next->from > absoluteSeconds) break;
        piece = next;
    }
    return evaluatePitchCurve(piece->points, absoluteSeconds);
}

std::vector<double> SharedPitchLine::cornersBetween(double from, double to) const
{
    std::vector<double> corners { from };
    for (std::size_t index = 0; index < pieces.size(); ++index)
    {
        const auto start = index == 0 ? -std::numeric_limits<double>::infinity()
                                      : pieces[index].from;
        const auto end = index + 1 < pieces.size() ? pieces[index + 1].from
                                                   : std::numeric_limits<double>::infinity();
        if (index > 0 && start > from && start < to) corners.push_back(start);
        for (const auto& point : pieces[index].points)
            if (point.timeSeconds > std::max(from, start) && point.timeSeconds < std::min(to, end))
                corners.push_back(point.timeSeconds);
    }
    corners.push_back(to);
    std::sort(corners.begin(), corners.end());
    corners.erase(std::unique(corners.begin(), corners.end(), [](double left, double right)
    {
        return std::abs(left - right) < 1.0e-9;
    }), corners.end());
    return corners;
}

static SharedPitchLines nativeSharedPitchLines(const TrackData& track,
    const juce::String& replacedNoteId, const std::vector<PitchCurveEditPoint>* replacedPoints,
    bool envelopeGrouping)
{
    struct Entry
    {
        juce::String id;
        double start, end;
        bool edited;
        bool independent;
        bool handlesPlaced;
        std::vector<PitchCurveEditPoint> points;
    };
    std::map<juce::String, std::vector<Entry>> groups;
    for (const auto& parent : track.clips)
        for (const auto& clip : expandedClipParts(parent))
            for (const auto& note : clip.notes)
            {
                if (note.nativeUnpitched) continue;
                const auto replaced = replacedPoints && note.id == replacedNoteId && !replacedPoints->empty();
                Entry entry {note.id, clip.startSeconds + note.startSeconds,
                    clip.startSeconds + note.startSeconds + note.durationSeconds,
                    replaced || !nativePitchIsUnedited(note), note.nativeIndependentPitch && !envelopeGrouping,
                    replaced || note.nativePitchHandlesPlaced, replaced ? *replacedPoints : ownPitchPoints(note)};
                for (auto& point : entry.points) point.timeSeconds += entry.start;
                const auto group = clip.nativePitchGroupId.isNotEmpty() ? clip.nativePitchGroupId
                    : parent.nativeAudioLinked ? parent.id : clip.id;
                groups[group].push_back(std::move(entry));
            }
    SharedPitchLines result;
    for (auto& [group, entries] : groups)
    {
        juce::ignoreUnused(group);
        std::stable_sort(entries.begin(), entries.end(), [](const auto& a, const auto& b)
        { return a.start < b.start; });
        for (std::size_t first = 0; first < entries.size();)
        {
            auto last = first;
            // Independent overlapping voices cannot be collapsed into one F0.
            while (last + 1 < entries.size() && !entries[last].independent && !entries[last + 1].independent
                && entries[last + 1].start >= entries[last].end - .002)
                ++last;
            if (last == first) { ++first; continue; }
            struct OwnedPoint { PitchCurveEditPoint point; std::size_t owner; };
            std::vector<OwnedPoint> points;
            bool edited = false;
            for (auto i = first; i <= last; ++i)
            {
                const auto& entry = entries[i]; edited |= entry.edited;
                for (std::size_t j = 0; j < entry.points.size(); ++j)
                {
                    const auto& point = entry.points[j];
                    // The internal cut is not a second note ending. The next
                    // section supplies its one joint point; real interior edits
                    // and the outermost endpoints remain independently editable.
                    if (i < last && j + 1 == entry.points.size()
                        && std::abs(point.timeSeconds - entry.end) < 1.0e-7) continue;
                    points.push_back({point, i});
                }
            }
            std::stable_sort(points.begin(), points.end(), [](const auto& a, const auto& b)
            { return a.point.timeSeconds < b.point.timeSeconds; });
            std::vector<OwnedPoint> unique;
            for (const auto& point : points)
            {
                if (!unique.empty() && std::abs(unique.back().point.timeSeconds - point.point.timeSeconds) < 1.0e-7)
                    unique.back() = point;
                else unique.push_back(point);
            }
            if (unique.empty()) { first = last + 1; continue; }
            auto line = std::make_shared<SharedPitchLine>();
            SharedPitchLine::Piece piece; piece.from = -std::numeric_limits<double>::infinity();
            for (const auto& point : unique) piece.points.push_back(point.point);
            line->pieces.push_back(std::move(piece));
            for (auto i = first; i <= last; ++i)
            {
                SharedPitchLineMember member; member.line = line;
                member.nativeSharedCurve = true; member.renderSharedCurve = edited;
                member.joinsPrevious = i > first; member.joinsNext = i < last;
                member.drawFrom = i == first ? std::min(entries[i].start, unique.front().point.timeSeconds) : entries[i].start;
                member.drawTo = i == last ? std::max(entries[i].end, unique.back().point.timeSeconds) : entries[i + 1].start;
                for (const auto& point : unique) if (point.owner == i) member.nativeHandleTimes.push_back(point.point.timeSeconds);
                result.byNote[entries[i].id] = std::move(member);
            }
            first = last + 1;
        }
        if (envelopeGrouping) continue;
        // Keep each independently edited body as its own interpolation piece.
        // Join touching bodies only in a one-control-point-unit window; never
        // feed their endpoints into a single whole-phrase natural spline.
        const auto ensureMember = [&](const Entry& entry) -> SharedPitchLineMember&
        {
            if (auto found = result.byNote.find(entry.id); found != result.byNote.end()) return found->second;
            auto line = std::make_shared<SharedPitchLine>();
            line->pieces.push_back({-std::numeric_limits<double>::infinity(), entry.points});
            SharedPitchLineMember member;
            member.line = line; member.nativeSharedCurve = true; member.renderSharedCurve = entry.edited;
            member.drawFrom = entry.points.empty() ? entry.start : std::min(entry.start,entry.points.front().timeSeconds);
            member.drawTo = entry.points.empty() ? entry.end : std::max(entry.end,entry.points.back().timeSeconds);
            for (const auto& point : entry.points) member.nativeHandleTimes.push_back(point.timeSeconds);
            return result.byNote.emplace(entry.id, std::move(member)).first->second;
        };
        for (std::size_t i = 1; i < entries.size(); ++i)
        {
            const auto& left = entries[i - 1]; const auto& right = entries[i];
            if (!(left.independent || right.independent) || std::abs(left.end - right.start) > 1.e-7) continue;
            if (!left.edited && !right.edited) continue; // Restoring source F0 is not a new glide edit.
            // Overlapping voices (including a third voice crossing the seam)
            // are independent even if two of their edges happen to touch.
            const auto seam = right.start;
            if (std::any_of(entries.begin(), entries.end(), [&](const auto& e)
                { return e.id != left.id && e.id != right.id && e.start < seam && e.end > seam; })) continue;
            const auto half = std::min({.0005, (left.end-left.start)*.25, (right.end-right.start)*.25});
            if (half <= 1.e-9) continue;
            // Only seed the original 1 ms bridge for automatic handles.
            // Placed endpoints themselves delimit the bridge; never recreate
            // a handle at the audio boundary after a horizontal drag.
            const auto from = left.independent && left.handlesPlaced && !left.points.empty()
                ? left.points.back().timeSeconds : seam-half;
            const auto to = right.independent && right.handlesPlaced && !right.points.empty()
                ? right.points.front().timeSeconds : seam+half;
            if (from > to) continue; // Invalid external/crossing data is not a reversed spline.
            auto& a = ensureMember(left); auto& b = ensureMember(right);
            const auto aLine = a.line, bLine = b.line;
            const auto low = aLine->midiAt(from), high = bLine->midiAt(to);
            auto joined = std::make_shared<SharedPitchLine>();
            for (const auto& piece : aLine->pieces)
                if (piece.from < from) joined->pieces.push_back(piece);
            PitchCurveEditPoint first {from, low}, last {to, high};
            first.shape = last.shape = PitchCurveShape::linear;
            joined->pieces.push_back({from, {first, last}});
            for (std::size_t p = 0; p < bLine->pieces.size(); ++p)
            {
                if (p+1 < bLine->pieces.size() && bLine->pieces[p+1].from <= to) continue;
                auto piece = bLine->pieces[p]; piece.from = std::max(to, piece.from);
                joined->pieces.push_back(std::move(piece));
            }
            for (auto& [id, member] : result.byNote)
                if (member.line == aLine || member.line == bLine)
                { member.line = joined; member.renderSharedCurve |= id == left.id || id == right.id; }
            a.joinsNext = true; b.joinsPrevious = true;
            a.drawTo = seam; b.drawFrom = seam;
            const auto boundaryHandle = [&](SharedPitchLineMember& member, const Entry& entry,
                                            double time, float midi, bool head)
            {
                if (!entry.independent || entry.handlesPlaced) return;
                if (member.nativeBoundaryAnchors.empty())
                {
                    member.nativeBoundaryAnchors = entry.points;
                    for (auto& p : member.nativeBoundaryAnchors) p.timeSeconds -= entry.start;
                }
                auto& anchors = member.nativeBoundaryAnchors;
                const auto local = time-entry.start;
                if (anchors.empty()) return;
                auto& point = head ? anchors.front() : anchors.back();
                point.timeSeconds = local; point.targetMidi = midi;
                member.nativeHandleTimes.clear();
                for (const auto& p : anchors) member.nativeHandleTimes.push_back(p.timeSeconds+entry.start);
            };
            boundaryHandle(a, left, from, low, false);
            boundaryHandle(b, right, to, high, true);
        }
    }
    return result;
}

SharedPitchLines sharedPitchLines(const TrackData& track, const juce::String& replacedNoteId,
                                  const std::vector<PitchCurveEditPoint>* replacedPoints, bool nativeEnvelopeGrouping)
{
    SharedPitchLines result;
    if (trackShowsAllNativeRegions(track))
        return nativeSharedPitchLines(track, replacedNoteId, replacedPoints, nativeEnvelopeGrouping);
    if (track.pitchAlgorithm != PitchAlgorithm::utau) return result;
    struct Entry
    {
        juce::String id;
        double start = 0.0;
        double end = 0.0;
        bool placed = false;
        bool rest = false;
        std::vector<PitchCurveEditPoint> points;   // absolute
    };
    std::vector<Entry> entries;
    for (const auto& clip : track.clips)
        for (const auto& note : clip.notes)
        {
            // A rest is sung by nobody, and nothing reaches across it.
            Entry entry;
            entry.rest = note.nativeUnpitched || backend::isRestLyric(note.label);
            entry.id = note.id;
            entry.start = clip.startSeconds + note.startSeconds;
            entry.end = entry.start + note.durationSeconds;
            const auto replaced = replacedPoints != nullptr && note.id == replacedNoteId
                && !replacedPoints->empty();
            entry.placed = replaced || !note.pitchControlPoints.empty();
            // In the order they are stored, not re-sorted: a UST can write a
            // bend whose widths run backwards, and its note has always been
            // sung by evaluating the points as they stand -- sorted, the same
            // points sing something else.  Its first point is the stored first,
            // as the renderer has always started a bend there.
            entry.points = replaced ? *replacedPoints : ownPitchPoints(note);
            for (auto& point : entry.points) point.timeSeconds += entry.start;
            if (!entry.points.empty()) entries.push_back(std::move(entry));
        }
    std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
    {
        if (std::abs(left.start - right.start) > 1.0e-9) return left.start < right.start;
        return left.end < right.end;
    });

    // Joined where the two notes' own points overlap in time, whichever way
    // round: the next note's placed bend begins inside this one, or this
    // note's placed bend runs on past where the next one starts.  Either way
    // one line crosses the boundary, and one line is what both notes are then
    // sung along -- a bend drawn past a note's end used to be drawn and never
    // sung, because nothing carried it into the note it reached into.
    // A UST's rests come in as gaps, and a rest this short is an articulation
    // rather than a break in the line: real songs bend over rests of 46 and
    // 60 ms.  A tenth of a second or more is a real gap.
    constexpr double shortRestSeconds = 0.08;
    const auto joined = [&entries](std::size_t index)
    {
        const auto& left = entries[index];
        const auto& right = entries[index + 1];
        const auto gap = right.start - left.end;
        // Never borrow a line through a rest, a real gap or an independent
        // overlapping voice. Zero-length lead-in notes may share a beat.
        if (left.rest || right.rest || gap < -0.002 || gap > shortRestSeconds)
            return false;
        if (right.placed && right.points.front().timeSeconds <= left.end + 1.0e-9)
            return true;
        // Over a short rest only a bend drawn on both sides joins them: a
        // curve carries on into a note with no points of its own only where
        // the two really touch.
        if (gap > 0.002 && !(left.placed && right.placed)) return false;
        auto source = index;
        while (!entries[source].placed && source > 0)
        {
            const auto& previous = entries[source - 1];
            if (previous.rest || std::abs(entries[source].start - previous.end) > 0.002)
                return false;
            --source;
        }
        const auto& edited = entries[source];
        if (!edited.placed) return false;
        const auto last = std::max_element(edited.points.begin(), edited.points.end(),
            [](const auto& one, const auto& other)
            {
                return one.timeSeconds < other.timeSeconds;
            });
        return last != edited.points.end() && last->timeSeconds > right.start + 1.0e-9;
    };

    std::size_t first = 0;
    while (first < entries.size())
    {
        auto last = first;
        while (last + 1 < entries.size() && joined(last)) ++last;
        if (last == first)
        {
            ++first;
            continue;
        }
        const auto count = last - first + 1;
        // An unedited neighbour is a reader, not an overriding edit. Preserve
        // the preceding explicit curve through its last point, even when that
        // point lies inside (or beyond) the next note. Do not alter stored data.
        for (auto index = first + 1; index <= last; ++index)
        {
            auto& next = entries[index];
            auto source = index - 1;
            while (source > first && !entries[source].placed) --source;
            const auto& previous = entries[source];
            if (next.placed || !previous.placed) continue;
            const auto end = std::max_element(previous.points.begin(), previous.points.end(),
                [](const auto& a, const auto& b) { return a.timeSeconds < b.timeSeconds; });
            if (end == previous.points.end() || end->timeSeconds <= next.start) continue;
            const auto fallbackMidi = next.points.back().targetMidi;
            std::erase_if(next.points, [&](const auto& p) { return p.timeSeconds <= end->timeSeconds; });
            if (next.points.empty())
                next.points.push_back({ std::max(next.end, end->timeSeconds + 0.02),
                                       fallbackMidi });
        }
        // Who owns what: from its first point on, the latest note whose first
        // point has been reached.  A note whose every moment a later note has
        // already reached owns nothing.
        std::vector<double> firstPoint(count), ownFrom(count), ownUntil(count);
        for (std::size_t index = 0; index < count; ++index)
            firstPoint[index] = entries[first + index].points.front().timeSeconds;
        auto laterFirst = std::numeric_limits<double>::infinity();
        for (auto index = count; index-- > 0;)
        {
            ownFrom[index] = index == 0 ? -std::numeric_limits<double>::infinity()
                                        : firstPoint[index];
            ownUntil[index] = laterFirst;   // where the next owner takes over
            laterFirst = std::min(laterFirst, firstPoint[index]);
        }

        // A note with no point of its own inside its stretch owns nothing:
        // it would only be holding a point it no longer has there.
        std::vector<double> lastPoint(count, -std::numeric_limits<double>::infinity());
        for (std::size_t index = 0; index < count; ++index)
        {
            auto any = false;
            // Up to and including the moment the next note takes over: the
            // two may stand on one vertical line, and both count there.
            for (const auto& point : entries[first + index].points)
                if (point.timeSeconds >= ownFrom[index] - 1.0e-9
                    && point.timeSeconds <= ownUntil[index] + 1.0e-9)
                {
                    lastPoint[index] = any ? std::max(lastPoint[index], point.timeSeconds)
                                           : point.timeSeconds;
                    any = true;
                }
            if (!any) ownUntil[index] = ownFrom[index];
        }

        auto shared = std::make_shared<SharedPitchLine>();
        for (std::size_t index = 0; index < count; ++index)
        {
            if (!(ownUntil[index] > ownFrom[index])) continue;
            const auto& own = entries[first + index].points;
            SharedPitchLine::Piece piece;
            // The first stretch holds its start for as long as before it.
            piece.from = shared->pieces.empty() ? -std::numeric_limits<double>::infinity()
                                                : ownFrom[index];
            piece.points = own;
            shared->pieces.push_back(std::move(piece));
            if (!std::isfinite(ownUntil[index])) continue;
            // From this note's last point there to the first point of the note
            // that takes over, smoothly -- in the shape that first point gives
            // its incoming run.  Both ends are points on screen.
            std::size_t taker = index + 1;
            while (taker < count && std::abs(firstPoint[taker] - ownUntil[index]) > 1.0e-12)
                ++taker;
            if (taker >= count) continue;
            const auto& next = entries[first + taker].points;
            SharedPitchLine::Piece join;
            join.from = lastPoint[index];
            PitchCurveEditPoint from { lastPoint[index], evaluatePitchCurve(own, lastPoint[index]) };
            PitchCurveEditPoint into = next.front();
            into.timeSeconds = ownUntil[index];
            into.targetMidi = evaluatePitchCurve(next, ownUntil[index]);
            join.points = { from, into };
            if (ownUntil[index] - lastPoint[index] > 1.0e-9)
                shared->pieces.push_back(std::move(join));
        }
        for (std::size_t index = 0; index < count; ++index)
        {
            SharedPitchLineMember member;
            member.line = shared;
            const auto& own = entries[first + index].points;
            member.ownFrom = ownFrom[index];
            // Up to and including the moment the next note takes over.
            member.ownTo = std::isfinite(ownUntil[index]) ? ownUntil[index]
                                                          : std::numeric_limits<double>::infinity();
            if (!(ownUntil[index] > ownFrom[index]))
                member.ownTo = member.ownFrom;   // owns nothing
            member.drawFrom = std::max(ownFrom[index], own.front().timeSeconds);
            member.drawTo = std::isfinite(ownUntil[index]) ? ownUntil[index]
                                                           : own.back().timeSeconds;
            if (member.drawTo < member.drawFrom || !(ownUntil[index] > ownFrom[index]))
                member.drawTo = member.drawFrom;   // nothing of its own to draw
            member.joinsPrevious = index > 0;
            member.joinsNext = index + 1 < count;
            member.takeover = ownUntil[index];
            result.byNote[entries[first + index].id] = std::move(member);
        }
        first = last + 1;
    }
    return result;
}

float evaluatePitchCurve(const std::vector<PitchCurveEditPoint>& points,
                         double timeSeconds)
{
    if (points.empty()) return 60.0f;
    if (points.size() == 1) return points.front().targetMidi;
    const auto right = std::upper_bound(points.begin(), points.end(), timeSeconds,
        [](double value, const PitchCurveEditPoint& point)
        {
            return value < point.timeSeconds;
        });
    if (right == points.begin()) return points.front().targetMidi;
    if (right == points.end()) return points.back().targetMidi;
    const auto rightIndex = static_cast<std::size_t>(right - points.begin());
    const auto leftIndex = rightIndex - 1;
    const auto& left = points[leftIndex];
    const auto& next = points[rightIndex];
    const auto span = next.timeSeconds - left.timeSeconds;
    if (span <= 1.0e-9) return next.targetMidi;
    const auto u = static_cast<float>(juce::jlimit(0.0, 1.0,
        (timeSeconds - left.timeSeconds) / span));

    auto shaped = u;
    switch (next.shape)
    {
        case PitchCurveShape::linear:
        case PitchCurveShape::smooth:
        case PitchCurveShape::easeIn:
        case PitchCurveShape::easeOut:
        case PitchCurveShape::customBezier:
            shaped = shapedSegmentProgress(next.shape, u, next.bezierX1,
                                           next.bezierY1, next.bezierX2,
                                           next.bezierY2);
            break;
        case PitchCurveShape::natural:
        {
            // A monotone cubic Hermite curve.  Interior tangents use the
            // Fritsch-Carlson weighted harmonic mean; note endpoints settle
            // horizontally.  This keeps adjacent segments C1-smooth without
            // introducing accidental overshoot.  With only two points it is
            // exactly a smooth S transition.
            std::vector<double> intervals(points.size() - 1);
            std::vector<float> secants(points.size() - 1);
            for (std::size_t index = 0; index + 1 < points.size(); ++index)
            {
                intervals[index] = std::max(1.0e-9,
                    points[index + 1].timeSeconds - points[index].timeSeconds);
                secants[index] = (points[index + 1].targetMidi
                    - points[index].targetMidi) / static_cast<float>(intervals[index]);
            }
            std::vector<float> tangents(points.size(), 0.0f);
            for (std::size_t index = 1; index + 1 < points.size(); ++index)
            {
                const auto before = secants[index - 1];
                const auto after = secants[index];
                if (before == 0.0f || after == 0.0f
                    || std::signbit(before) != std::signbit(after))
                    continue;
                const auto firstWeight = 2.0 * intervals[index] + intervals[index - 1];
                const auto secondWeight = intervals[index] + 2.0 * intervals[index - 1];
                tangents[index] = static_cast<float>((firstWeight + secondWeight)
                    / (firstWeight / before + secondWeight / after));
            }
            const auto h00 = 2.0f * u * u * u - 3.0f * u * u + 1.0f;
            const auto h10 = u * u * u - 2.0f * u * u + u;
            const auto h01 = -2.0f * u * u * u + 3.0f * u * u;
            const auto h11 = u * u * u - u * u;
            return h00 * left.targetMidi
                + h10 * static_cast<float>(span) * tangents[leftIndex]
                + h01 * next.targetMidi
                + h11 * static_cast<float>(span) * tangents[rightIndex];
        }
    }
    return left.targetMidi + (next.targetMidi - left.targetMidi) * shaped;
}

juce::String pitchCurveShapeName(PitchCurveShape value)
{
    switch (value)
    {
        case PitchCurveShape::natural: return "natural";
        case PitchCurveShape::linear: return "linear";
        case PitchCurveShape::smooth: return "smooth";
        case PitchCurveShape::easeIn: return "ease-in";
        case PitchCurveShape::easeOut: return "ease-out";
        case PitchCurveShape::customBezier: return "custom-bezier";
    }
    return "natural";
}

PitchCurveShape parsePitchCurveShape(const juce::String& value)
{
    if (value == "linear") return PitchCurveShape::linear;
    if (value == "smooth") return PitchCurveShape::smooth;
    if (value == "ease-in") return PitchCurveShape::easeIn;
    if (value == "ease-out") return PitchCurveShape::easeOut;
    if (value == "custom-bezier") return PitchCurveShape::customBezier;
    return PitchCurveShape::natural;
}

// The two above are named in the project file and over MCP, so they belong to
// the interface; everything below is this file's own business.
namespace
{
juce::String pitchAlgorithmName(PitchAlgorithm value)
{
    switch (value)
    {
        case PitchAlgorithm::mld5: return "mld5";
        case PitchAlgorithm::mld3: return "mld3";
        case PitchAlgorithm::nsfHifigan: return "nsf-hifigan";
        case PitchAlgorithm::world: return "world";
        case PitchAlgorithm::vocalShifter: return "vslib";
        case PitchAlgorithm::llsm2: return "llsm2";
        case PitchAlgorithm::utau: return "utau";
    }
    return "llsm2";
}

PitchAlgorithm parsePitchAlgorithm(const juce::String& value)
{
    if (value == "nsf-hifigan") return PitchAlgorithm::nsfHifigan;
    if (value == "mld3") return PitchAlgorithm::mld3;
    if (value == "mld5") return PitchAlgorithm::mld5;
    if (value == "world") return PitchAlgorithm::world;
    if (value == "vslib") return PitchAlgorithm::vocalShifter;
    if (value == "llsm2") return PitchAlgorithm::llsm2;
    if (value == "utau") return PitchAlgorithm::utau;
    return PitchAlgorithm::llsm2;
}

juce::String stretchAlgorithmName(StretchAlgorithm value)
{
    switch (value)
    {
        case StretchAlgorithm::melodyneHybrid: return "melodyne-hybrid";
        case StretchAlgorithm::variableMelHop: return "variable-mel-hop";
        case StretchAlgorithm::loop: return "loop";
        case StretchAlgorithm::soundTouch: return "soundtouch";
        case StretchAlgorithm::nsfShiftThenSplice: return "nsf-shift-then-splice";
        case StretchAlgorithm::hifiShifterMel: return "hifishifter-mel";
    }
    return "melodyne-hybrid";
}

StretchAlgorithm parseStretchAlgorithm(const juce::String& value)
{
    if (value == "variable-mel-hop") return StretchAlgorithm::variableMelHop;
    if (value == "loop") return StretchAlgorithm::loop;
    if (value == "soundtouch") return StretchAlgorithm::soundTouch;
    if (value == "nsf-shift-then-splice") return StretchAlgorithm::nsfShiftThenSplice;
    if (value == "hifishifter-mel") return StretchAlgorithm::hifiShifterMel;
    return StretchAlgorithm::melodyneHybrid;
}

juce::String renderOrderName(RenderOrder value)
{
    switch (value)
    {
        case RenderOrder::processThenSplice: return "process-then-splice";
        case RenderOrder::stretchSpliceThenPitch: return "stretch-splice-then-pitch";
    }
    return "process-then-splice";
}

RenderOrder parseRenderOrder(const juce::String& value)
{
    if (value == "stretch-splice-then-pitch") return RenderOrder::stretchSpliceThenPitch;
    return RenderOrder::processThenSplice;
}
}

double ProjectData::durationSeconds() const
{
    double duration = 8.0;
    for (const auto& track : tracks)
        for (const auto& clip : track.clips)
            duration = std::max(duration, clip.startSeconds + clip.durationSeconds);
    return duration;
}

double ProjectData::secondsForQuarterPosition(double quarterPosition) const
{
    const auto baseTempo = juce::jlimit(20.0, 400.0, bpm);
    if (quarterPosition <= 0.0)
        return beatOriginSeconds + quarterPosition * 60.0 / baseTempo;

    auto seconds = beatOriginSeconds;
    auto previousQuarter = 0.0;
    auto tempo = baseTempo;
    for (const auto& change : tempoChanges)
    {
        const auto changeQuarter = std::max(0.0, change.quarterPosition);
        if (changeQuarter <= previousQuarter + 1.0e-9)
        {
            tempo = juce::jlimit(20.0, 400.0, change.bpm);
            continue;
        }
        if (quarterPosition <= changeQuarter)
            return seconds + (quarterPosition - previousQuarter) * 60.0 / tempo;
        seconds += (changeQuarter - previousQuarter) * 60.0 / tempo;
        previousQuarter = changeQuarter;
        tempo = juce::jlimit(20.0, 400.0, change.bpm);
    }
    return seconds + (quarterPosition - previousQuarter) * 60.0 / tempo;
}

double ProjectData::quarterPositionForSeconds(double targetSeconds) const
{
    const auto baseTempo = juce::jlimit(20.0, 400.0, bpm);
    if (targetSeconds <= beatOriginSeconds)
        return (targetSeconds - beatOriginSeconds) * baseTempo / 60.0;

    auto seconds = beatOriginSeconds;
    auto previousQuarter = 0.0;
    auto tempo = baseTempo;
    for (const auto& change : tempoChanges)
    {
        const auto changeQuarter = std::max(0.0, change.quarterPosition);
        if (changeQuarter <= previousQuarter + 1.0e-9)
        {
            tempo = juce::jlimit(20.0, 400.0, change.bpm);
            continue;
        }
        const auto changeSeconds = seconds
            + (changeQuarter - previousQuarter) * 60.0 / tempo;
        if (targetSeconds <= changeSeconds)
            return previousQuarter + (targetSeconds - seconds) * tempo / 60.0;
        seconds = changeSeconds;
        previousQuarter = changeQuarter;
        tempo = juce::jlimit(20.0, 400.0, change.bpm);
    }
    return previousQuarter + (targetSeconds - seconds) * tempo / 60.0;
}

double ProjectData::tempoAtQuarterPosition(double quarterPosition) const
{
    auto tempo = juce::jlimit(20.0, 400.0, bpm);
    for (const auto& change : tempoChanges)
    {
        if (change.quarterPosition > quarterPosition + 1.0e-9) break;
        tempo = juce::jlimit(20.0, 400.0, change.bpm);
    }
    return tempo;
}

double ProjectData::tempoAtSeconds(double seconds) const
{
    return tempoAtQuarterPosition(quarterPositionForSeconds(seconds));
}

ProjectModel::ProjectModel()
{
    project.name = "Untitled";
}

ProjectData ProjectModel::snapshot() const
{
    const juce::ScopedLock guard(lock);
    return project;
}

std::uint64_t ProjectModel::revisionNumber() const
{
    const juce::ScopedLock guard(lock);
    return revision;
}

void ProjectModel::pushUndoLocked()
{
    if (suppressNestedUndo) return;
    undoHistory.push_back(project);
    if (undoHistory.size() > maxHistory)
        undoHistory.erase(undoHistory.begin());
    redoHistory.clear();
    ++revision;
}

void ProjectModel::replace(ProjectData replacement)
{
    replaceInternal(std::move(replacement), false);
}

void ProjectModel::resetDocument(ProjectData replacement)
{
    replaceInternal(std::move(replacement), true);
}

void ProjectModel::replaceInternal(ProjectData replacement, bool newDocument)
{
    for (auto& track : replacement.tracks)
    {
        if (track.accompaniment) { track.compose = false; track.referenceOnly = false; }
        else if (track.voicebankDirectory.getChildFile("dsconfig.yaml").existsAsFile()) track.utauMode = UtauMode::mou;
    }
    {
        const juce::ScopedLock guard(lock);
        if (newDocument) { undoHistory.clear(); redoHistory.clear(); ++revision; }
        else pushUndoLocked();
        project = std::move(replacement);
    }
    sendChangeMessage();
}

bool ProjectModel::setHamoodState(const juce::String& state, juce::String& error)
{
    const auto parsed=juce::JSON::parse(state);
    if(!hamoodstate::validate(parsed,error))return false;
    const auto canonical=hamoodstate::encode(parsed);
    {const juce::ScopedLock guard(lock);if(project.hamoodState==canonical)return true;pushUndoLocked();project.hamoodState=canonical;}
    sendChangeMessage();return true;
}

void ProjectModel::clear()
{
    resetDocument();
}

bool ProjectModel::undo()
{
    {
        const juce::ScopedLock guard(lock);
        if (undoHistory.empty()) return false;
        redoHistory.push_back(project);
        if (redoHistory.size() > maxHistory) redoHistory.erase(redoHistory.begin());
        project = std::move(undoHistory.back());
        undoHistory.pop_back();
        ++revision;
    }
    sendChangeMessage();
    return true;
}

bool ProjectModel::redo()
{
    {
        const juce::ScopedLock guard(lock);
        if (redoHistory.empty()) return false;
        undoHistory.push_back(project);
        if (undoHistory.size() > maxHistory) undoHistory.erase(undoHistory.begin());
        project = std::move(redoHistory.back());
        redoHistory.pop_back();
        ++revision;
    }
    sendChangeMessage();
    return true;
}

bool ProjectModel::canUndo() const
{
    const juce::ScopedLock guard(lock);
    return !undoHistory.empty();
}

bool ProjectModel::canRedo() const
{
    const juce::ScopedLock guard(lock);
    return !redoHistory.empty();
}

juce::String ProjectModel::makeId(const char* prefix)
{
    return juce::String(prefix) + "_" + juce::Uuid().toString().removeCharacters("-");
}

juce::String ProjectModel::addAudioFile(const juce::File& file, double durationSeconds,
                                        double startSeconds,
                                        const juce::String& targetTrackId)
{
    ClipData clip;
    clip.id = makeId("clip");
    clip.sourceFile = file;
    clip.startSeconds = std::max(0.0, startSeconds);
    clip.durationSeconds = std::max(0.01, durationSeconds);
    clip.sourceDurationSeconds = clip.durationSeconds;

    // Voicebank registration writes timing beside the source before the file
    // is dragged into a project.  Consume that sidecar immediately so an OTO
    // sample arrives as editable note objects instead of a silent/empty piano
    // roll that requires drawing every region again.
    const auto current = snapshot();
    const auto accompaniment = std::any_of(current.tracks.begin(), current.tracks.end(),
        [&](const auto& track) { return track.id == targetTrackId && track.accompaniment; });
    const auto sidecar = SampleSettings::sidecarFor(file);
    const juce::File legacy(file.getFullPathName() + ".hachi.csv");
    if (!accompaniment && (sidecar.existsAsFile() || legacy.existsAsFile()))
    {
        const auto rows = SampleSettings::loadOrDerive(file, ProjectData{});
        std::vector<RegionSpan> spans;
        spans.reserve(rows.size());
        for (const auto& row : rows)
        {
            const auto regionStart = juce::jlimit(0.0, clip.durationSeconds,
                                                   row.regionStartSeconds);
            const auto regionEnd = juce::jlimit(regionStart, clip.durationSeconds,
                                                 row.regionEndSeconds);
            spans.push_back({ regionStart,
                              regionEnd - regionStart < 0.001 ? regionStart : regionEnd });
        }
        for (const auto index : regionsToImport(spans))
        {
            const auto& row = rows[index];
            const auto regionStart = spans[index].startSeconds;
            const auto regionEnd = spans[index].endSeconds;
            if (regionEnd - regionStart < 0.001) continue;
            NoteData note;
            note.id = makeId("note");
            note.label = row.name.trim().isEmpty() ? "-" : row.name.trim();
            note.nativeRole = row.role;
            note.nativeUnpitched = row.nativeUnpitched;
            note.nativeProvenance = row.provenance;
            note.nativeConfidence = row.confidence;
            note.nativeSourceStartSeconds = row.regionStartSeconds;
            note.nativeSourceEndSeconds = row.regionEndSeconds;
            note.startSeconds = regionStart;
            note.durationSeconds = regionEnd - regionStart;
            note.consonantSeconds = juce::jlimit(0.0, note.durationSeconds,
                                                  row.fixedDurationSeconds);
            const auto storedSource = row.melodyneOriginalPitchCenterCents > 0.0
                ? static_cast<float>(row.melodyneOriginalPitchCenterCents / 100.0)
                : row.melodynePitchCenterCents > 0.0
                    ? static_cast<float>(row.melodynePitchCenterCents / 100.0)
                    : 60.0f;
            note.sourceMidiCenter = row.melodyneOriginalPitchCenterCents > 0.0
                || row.melodynePitchCenterCents > 0.0
                ? juce::jlimit(0.0f, 127.0f, storedSource) : -1.0f;
            note.midiNote = row.melodynePitchCenterCents > 0.0
                ? juce::jlimit(0.0f, 127.0f,
                    static_cast<float>(row.melodynePitchCenterCents / 100.0))
                : juce::jlimit(0.0f, 127.0f, storedSource
                    + static_cast<float>(row.relativePitchCents / 100.0));
            note.drift = juce::jlimit(0.0f, 2.0f,
                static_cast<float>(row.melodynePitchDrift));
            note.modulation = juce::jlimit(0.0f, 2.0f,
                static_cast<float>(row.melodynePitchModulation));
            note.formantSemitones = juce::jlimit(-12.0f, 12.0f,
                static_cast<float>(row.melodyneFormantCents / 100.0));
            note.breath = juce::jlimit(0.0f, 1.0f,
                static_cast<float>(row.melodyneSibilantBalance));
            note.gain = juce::jlimit(0.0f, 4.0f,
                static_cast<float>(row.melodyneAmplitude));
            note.attackSpeed = juce::jlimit(0.05f, 20.0f,
                static_cast<float>(row.melodyneAttackSeconds > 1.0e-6
                    ? row.fixedDurationSeconds / row.melodyneAttackSeconds : 1.0));
            note.nativeSegments = SampleSettings::nativeSegmentsFor(row);
            note.amplitudeEnvelope = row.amplitudeEnvelope;
            note.utauPreutteranceOverrideEnabled = true;
            note.utauPreutteranceSeconds = std::max(0.0,
                row.alignmentSeconds - row.regionStartSeconds);
            note.utauOverlapOverrideEnabled = std::abs(row.overlapSeconds) > 1.0e-9;
            note.utauOverlapSeconds = row.overlapSeconds;
            // Sidecars supply authored regions and controls. This temporary
            // contour is replaced by acoustic F0 when import analysis finishes.
            note.contour.push_back({ 0.0, 0.0f, 0.0f, !note.nativeUnpitched });
            note.contour.push_back({ note.durationSeconds, 0.0f, 0.0f, !note.nativeUnpitched });
            if (note.nativeUnpitched)
            { note.sourceMidiCenter = note.midiNote; note.sourcePitchMeasured = true; note.nativeIndependentPitch = true; note.utauAutoPitchTransition = false; }
            clip.notes.push_back(std::move(note));
        }
    }
    const auto clipId = clip.id;

    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        const auto target = std::find_if(project.tracks.begin(), project.tracks.end(),
            [&targetTrackId](const auto& track)
            {
                return targetTrackId.isNotEmpty() && track.id == targetTrackId;
            });
        if (target != project.tracks.end())
        {
            if (!trackShowsAllNativeRegions(*target))
                std::erase_if(clip.notes, [](const auto& n) { return n.nativeUnpitched; });
            if (trackIsDiffSinger(*target)) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
            auto relative=clip;relative.startSeconds=0;clip.startSeconds=nativePasteStart(*target,{relative},clip.startSeconds);
            target->clips.push_back(std::move(clip));
        }
        else
        {
            TrackData track;
            track.id = makeId("track");
            track.name = file.getFileNameWithoutExtension();
            track.clips.push_back(std::move(clip));
            project.tracks.push_back(std::move(track));
        }
        if (project.name == "Untitled")
            project.name = file.getFileNameWithoutExtension();
    }
    sendChangeMessage();
    return clipId;
}

juce::String ProjectModel::addTrack(const juce::String& requestedName, bool compose,
                                   bool referenceOnly, bool accompaniment)
{
    juce::String id;
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        TrackData track;
        track.id = makeId("track");
        track.name = requestedName.trim().substring(0, 80);
        if (track.name.isEmpty())
            track.name = accompaniment ? "Accompaniment" : compose ? "Melodic Track" : "Audio Track";
        track.accompaniment = accompaniment;
        track.compose = compose && !accompaniment;
        track.referenceOnly = referenceOnly && !accompaniment;
        // A new track follows the currently established project workflow,
        // rather than unexpectedly returning to mld5 after the user has
        // selected NSF/WORLD or a different stretch engine.
        if (!accompaniment)
        {
            const auto previous = std::find_if(project.tracks.rbegin(), project.tracks.rend(),
                [](const auto& candidate) { return !candidate.accompaniment; });
            if (previous != project.tracks.rend())
            {
                track.pitchAlgorithm = previous->pitchAlgorithm;
                track.stretchAlgorithm = previous->stretchAlgorithm;
                track.renderOrder = previous->renderOrder;
            }
        }
        id = track.id;
        project.tracks.push_back(std::move(track));
    }
    sendChangeMessage();
    return id;
}

void ProjectModel::setTrackReferenceOnly(const juce::String& trackId,
                                         bool referenceOnly)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.referenceOnly != referenceOnly)
            {
                pushUndoLocked();
                track.referenceOnly = referenceOnly;
                changed = true;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackName(const juce::String& trackId,
                                const juce::String& requestedName)
{
    const auto name = requestedName.trim().substring(0, 80);
    if (name.isEmpty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.name != name)
            {
                pushUndoLocked();
                track.name = name;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::setClipNotesIfEmpty(const juce::String& clipId,
                                       std::vector<NoteData> notes)
{
    if (notes.empty()) return false;
    auto changed = false;
    juce::File annotationSource;
    std::vector<SampleRegionSetting> annotationRows;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            for (auto& clip : track.clips)
                if (!track.accompaniment && clip.id == clipId && clip.notes.empty())
                {
                    // Import analysis is one operation.  More importantly,
                    // do not replace notes the user drew while it was running.
                    annotationSource = clip.sourceFile;
                    const auto ratio = clip.durationSeconds > 1.0e-9
                        ? clip.sourceDurationSeconds / clip.durationSeconds : 1.0;
                    annotationRows.reserve(notes.size());
                    for (auto& note : notes)
                    {
                        if (trackIsDiffSinger(track)) note.utauFlagCurveEnabled = true;
                        if (note.label.trim().isEmpty()) note.label = "-";
                        note.nativeRole = note.label == "_"
                            ? NativeSegmentRole::transition : NativeSegmentRole::unknown;
                        note.nativeProvenance = note.nativeUnpitched ? "uncovered-audio" : "estimated";
                        note.nativeConfidence = 0.0f;
                        note.nativeSourceStartSeconds = clip.sourceOffsetSeconds
                            + note.startSeconds * ratio;
                        note.nativeSourceEndSeconds = note.nativeSourceStartSeconds
                            + note.durationSeconds * ratio;
                        note.nativeSegments = { { "segment_1", note.label,
                            note.nativeRole, 0.0, note.durationSeconds,
                            "estimated", 0.0f, note.durationSeconds, 0.0,
                            true, 1.0 } };
                        SampleRegionSetting row;
                        row.name = note.label;
                        row.role = note.nativeRole;
                        row.nativeUnpitched = note.nativeUnpitched;
                        row.provenance = "estimated";
                        row.confidence = 0.0f;
                        row.regionStartSeconds = clip.sourceOffsetSeconds
                            + note.startSeconds * ratio;
                        row.regionEndSeconds = row.regionStartSeconds
                            + note.durationSeconds * ratio;
                        row.segments.push_back({ "segment_1", note.label,
                            note.nativeRole, row.regionStartSeconds,
                            row.regionEndSeconds, "estimated", 0.0f,
                            row.regionEndSeconds, 0.0, true, 1.0 });
                        annotationRows.push_back(std::move(row));
                    }
                    clip.notes = std::move(notes);
                    changed = true;
                    break;
                }
            if (changed) break;
        }
    }
    if (changed && annotationSource.existsAsFile())
    {
        juce::String annotationError;
        if (!SampleSettings::save(annotationSource, annotationRows, annotationError))
            DBG("Could not save analysed HJM annotation: " + annotationError);
    }
    if (changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::setNativeTrimSourceAnalysis(const juce::File& file,const std::vector<NoteData>& sourceNotes)
{
    if(sourceNotes.empty())return false;
    const auto reference=nativeSourcePitchReference(sourceNotes,[](double t){return t;});
    if(reference->empty())return false;
    bool changed=false;
    {
        const juce::ScopedLock guard(lock);
        for(auto& track:project.tracks)if(trackShowsAllNativeRegions(track))
            for(auto& parent:track.clips)
            {
                auto parts=expandedClipParts(parent);
                for(auto& part:parts)if(part.sourceFile==file)
                {
                    const auto pending=part.nativeSourcePitchPending;
                    part.nativeSourcePitch=reference;part.nativeSourcePitchComplete=true;part.nativeSourcePitchPending=false;
                    if(pending)for(auto& note:part.notes)refreshNativeTrimPitch(note,part);
                    if(parent.parts.empty())
                    {parent.nativeSourcePitch=reference;parent.nativeSourcePitchComplete=true;parent.nativeSourcePitchPending=false;}
                    else for(auto& original:parent.parts)if(parent.id+":"+original.id==part.id)
                    {original.nativeSourcePitch=reference;original.nativeSourcePitchComplete=true;original.nativeSourcePitchPending=false;}
                    if(pending)for(auto& note:parent.notes)
                        if(const auto found=std::find_if(part.notes.begin(),part.notes.end(),[&](const auto& n){return n.id==note.id;});found!=part.notes.end())
                        {note.contour=found->contour;note.sourceMidiCenter=found->sourceMidiCenter;note.sourcePitchMeasured=found->sourcePitchMeasured;}
                    changed=true;
                }
            }
    }
    if(changed)sendChangeMessage();return changed;
}

std::size_t ProjectModel::markNativeUnpitchedRegions(const juce::String& clipId)
{
    std::size_t added = 0;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks) if (trackShowsAllNativeRegions(track))
            for (auto& clip : track.clips) if (clip.id == clipId)
            {
                auto updated = clip;
                if (clip.parts.empty()) added = appendNativeUnpitchedRegions(updated);
                else for (auto part : expandedClipParts(clip))
                {
                    const auto before = part.notes;
                    added += appendNativeUnpitchedRegions(part);
                    for (auto note : part.notes)
                        if (std::none_of(before.begin(), before.end(), [&](const auto& n) { return n.id == note.id; }))
                        {
                            note.startSeconds += part.startSeconds - clip.startSeconds;
                            note.clipPartId = part.id.substring(clip.id.length() + 1);
                            updated.notes.push_back(std::move(note));
                        }
                }
                if (added) { pushUndoLocked(); clip = std::move(updated); }
                break;
            }
    }
    if (added) sendChangeMessage();
    return added;
}

bool ProjectModel::setClipAudioAnalysis(const juce::String& clipId,
                                        std::vector<NoteData> notes,
                                        const ClipData& importedClip)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
            {
                if (clip.id != clipId || track.accompaniment || !clip.parts.empty()) continue;
                const auto& imported = importedClip.id.isEmpty() ? clip : importedClip;
                if (clip.sourceFile != imported.sourceFile) continue;
                const auto native = !trackUsesVoicebankSynthesis(track) && !trackIsDiffSinger(track);
                if (imported.notes.empty())
                {
                    // Analysis cannot replace notes drawn while it was pending.
                    if (!clip.notes.empty()) return false;
                    if (!native) return setClipNotesIfEmpty(clipId, std::move(notes));
                    clip.nativeSourcePitch=nativeSourcePitchReference(notes,[](double t){return t;});
                    clip.nativeSourcePitchComplete=true;
                    const auto audio = nativeAudioPreviewClip(clip);
                    const auto map = nativeSourceTimeMap(audio);
                    const auto sourceEnd = audio.sourceOffsetSeconds + audio.sourceDurationSeconds;
                    auto mapped = clip;
                    for (auto note : notes)
                    {
                        const auto first = std::max(audio.sourceOffsetSeconds, note.startSeconds);
                        const auto last = std::min(sourceEnd, note.startSeconds + note.durationSeconds);
                        if (last - first < 0.001) continue;
                        const auto targetStart = nativeTargetTimeAt(map, first - audio.sourceOffsetSeconds);
                        const auto targetEnd = nativeTargetTimeAt(map, last - audio.sourceOffsetSeconds);
                        const auto attackEnd = nativeTargetTimeAt(map, juce::jlimit(0.0,
                            audio.sourceDurationSeconds, note.startSeconds + note.consonantSeconds
                                - audio.sourceOffsetSeconds));
                        const auto sourceAttack = std::max(0.0,
                            std::min(last, note.startSeconds + note.consonantSeconds) - first);
                        note.startSeconds = clip.audioStartSeconds + targetStart;
                        note.durationSeconds = std::max(0.001, targetEnd - targetStart);
                        note.consonantSeconds = juce::jlimit(0.0, note.durationSeconds, attackEnd - targetStart);
                        if (note.consonantSeconds > 1.0e-9)
                            note.attackSpeed = juce::jlimit(0.05f, 20.0f,
                                static_cast<float>(sourceAttack / note.consonantSeconds));
                        note.contour.clear();
                        mapped.notes.push_back(std::move(note));
                    }
                    appendNativeUnpitchedRegions(mapped);
                    backend::AnalysisService::applySourcePitch(mapped, notes);
                    for (auto& note : mapped.notes)
                    {
                        note.midiNote = note.sourceMidiCenter;
                        // Acoustic breath evidence describes the original
                        // recording; it is not a request to add more breath.
                        note.breath = 0.0f;
                    }
                    return setClipNotesIfEmpty(clipId, std::move(mapped.notes));
                }
                if (!native) return false;
                auto measured = clip;
                std::erase_if(measured.notes, [&](const auto& note)
                {
                    return std::none_of(imported.notes.begin(), imported.notes.end(),
                        [&](const auto& original) { return original.id == note.id; });
                });
                if (backend::AnalysisService::applySourcePitch(measured, notes) == 0) return false;
                clip.nativeSourcePitch=measured.nativeSourcePitch;
                clip.nativeSourcePitchComplete=measured.nativeSourcePitchComplete;
                for (auto& note : clip.notes)
                    if (const auto found = std::find_if(measured.notes.begin(), measured.notes.end(),
                        [&](const auto& measuredNote) { return measuredNote.id == note.id; });
                        found != measured.notes.end())
                    {
                        note.sourceMidiCenter = found->sourceMidiCenter;
                        note.sourcePitchMeasured = found->sourcePitchMeasured;
                        note.midiNote = found->midiNote;
                        note.contour = std::move(found->contour);
                    }
                // Do not refill gaps in a clip already changed by the user.
                if (clip.startSeconds == imported.startSeconds && clip.durationSeconds == imported.durationSeconds
                    && clip.sourceOffsetSeconds == imported.sourceOffsetSeconds
                    && clip.sourceDurationSeconds == imported.sourceDurationSeconds
                    && clip.notes.size() == imported.notes.size())
                    appendNativeUnpitchedRegions(clip);
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::writeMidiFile(const ProjectData& data, const juce::File& file,
                                 juce::String& error)
{
    // Ticks, because that is what a MIDI file counts in and what keeps the
    // song on its beats wherever it is opened.  480 to the quarter is what a
    // UST counts in as well, so a note written as a whole number of ticks
    // there comes back out as one here.
    constexpr auto ticksPerQuarter = 480;
    const auto tickOf = [&data](double seconds)
    {
        // Tick zero is the project's first beat.  Anything before it -- a
        // note dragged ahead of the timeline's origin -- has nowhere to go in
        // a MIDI file, which starts where it starts.
        return juce::jmax(0, juce::roundToInt(
            data.quarterPositionForSeconds(seconds) * ticksPerQuarter));
    };

    juce::MidiFile midi;
    midi.setTicksPerQuarterNote(ticksPerQuarter);

    // The conductor track: what the song is called, how fast it goes and how
    // it is counted.  Its own track, as a type 1 file wants it.
    juce::MidiMessageSequence conductor;
    conductor.addEvent(juce::MidiMessage::textMetaEvent(3, data.name), 0.0);
    conductor.addEvent(juce::MidiMessage::timeSignatureMetaEvent(
        juce::jmax(1, data.numerator), juce::jmax(1, data.denominator)), 0.0);
    const auto tempoEvent = [](double bpm)
    {
        return juce::MidiMessage::tempoMetaEvent(juce::roundToInt(
            60.0e6 / juce::jlimit(20.0, 400.0, bpm)));
    };
    // The speed, and every change of it, at one event per tick: a change
    // written at the very start is the song's speed rather than a second
    // answer beside it -- which is what a UST whose first note carries a
    // tempo of its own leaves behind.  Readers that take the first of two
    // would play the whole song at a speed nothing is sung at.
    std::vector<std::pair<double, double>> speeds { { 0.0, data.bpm } };
    for (const auto& change : data.tempoChanges)
        speeds.emplace_back(juce::jmax(0.0, change.quarterPosition) * ticksPerQuarter,
                            change.bpm);
    std::stable_sort(speeds.begin(), speeds.end(),
                     [](const auto& left, const auto& right)
                     { return left.first < right.first; });
    for (std::size_t index = 0; index < speeds.size(); ++index)
    {
        if (index + 1 < speeds.size()
            && std::abs(speeds[index + 1].first - speeds[index].first) < 1.0e-9)
            continue;
        conductor.addEvent(tempoEvent(speeds[index].second), speeds[index].first);
    }
    conductor.updateMatchedPairs();
    midi.addTrack(conductor);

    auto notesWritten = 0;
    for (const auto& track : data.tracks)
    {
        struct Written
        {
            int start = 0, end = 0, number = 0, velocity = 100;
            juce::String lyric;
        };
        std::vector<Written> notes;
        for (const auto& clip : track.clips)
            for (const auto& note : clip.notes)
            {
                if (note.nativeUnpitched) continue;
                Written written;
                written.start = tickOf(clip.startSeconds + note.startSeconds);
                written.end = juce::jmax(written.start + 1,
                    tickOf(clip.startSeconds + note.startSeconds + note.durationSeconds));
                written.number = juce::jlimit(0, 127, juce::roundToInt(note.midiNote));
                // A note's loudness here is a multiplier of its own level and
                // is 1 almost everywhere; velocity 100 is what a UST calls
                // that, so the two agree about what "as written" means.
                written.velocity = juce::jlimit(1, 127,
                    juce::roundToInt(note.gain * 100.0f));
                written.lyric = note.label.trim();
                notes.push_back(std::move(written));
            }
        if (notes.empty()) continue;
        std::stable_sort(notes.begin(), notes.end(),
                         [](const Written& left, const Written& right)
                         {
                             if (left.start != right.start) return left.start < right.start;
                             return left.number < right.number;
                         });
        // Two notes of the same pitch that overlap are one note-off short of
        // each other: whichever off arrives first ends both, and the second
        // hangs to the end of the song.  The earlier one gives way.
        for (std::size_t index = 0; index + 1 < notes.size(); ++index)
            for (auto later = index + 1; later < notes.size(); ++later)
            {
                if (notes[later].start >= notes[index].end) break;
                if (notes[later].number != notes[index].number) continue;
                notes[index].end = juce::jmax(notes[index].start + 1, notes[later].start);
                break;
            }

        juce::MidiMessageSequence sequence;
        sequence.addEvent(juce::MidiMessage::textMetaEvent(3, track.name), 0.0);
        for (const auto& note : notes)
        {
            // The lyric sits with the note it is sung on, which is where every
            // reader of a singing MIDI looks for it.
            if (note.lyric.isNotEmpty())
                sequence.addEvent(juce::MidiMessage::textMetaEvent(5, note.lyric),
                                  note.start);
            sequence.addEvent(juce::MidiMessage::noteOn(1, note.number,
                static_cast<juce::uint8>(note.velocity)), note.start);
            sequence.addEvent(juce::MidiMessage::noteOff(1, note.number), note.end);
            ++notesWritten;
        }
        sequence.updateMatchedPairs();
        midi.addTrack(sequence);
    }

    if (notesWritten == 0)
    {
        error = "this project has no notes to write";
        return false;
    }
    juce::TemporaryFile temporary(file);
    {
        // Written beside the target and moved into place, so a write that
        // fails partway leaves the file that was there untouched.
        std::unique_ptr<juce::FileOutputStream> stream(
            temporary.getFile().createOutputStream());
        if (stream == nullptr || !midi.writeTo(*stream))
        {
            error = "could not write the MIDI file: " + file.getFullPathName();
            return false;
        }
    }
    if (!temporary.overwriteTargetFileWithTemporary())
    {
        error = "could not write the MIDI file: " + file.getFullPathName();
        return false;
    }
    return true;
}

bool ProjectModel::addMidiFile(const juce::File& file, juce::String& error)
{
    auto input = file.createInputStream();
    if (input == nullptr)
    {
        error = "Could not open MIDI file: " + file.getFullPathName();
        return false;
    }
    juce::MidiFile midi;
    if (!midi.readFrom(*input))
    {
        error = "Invalid MIDI file: " + file.getFullPathName();
        return false;
    }
    std::optional<double> importedBpm;
    for (int trackIndex = 0; trackIndex < midi.getNumTracks() && !importedBpm; ++trackIndex)
        if (const auto* sequence = midi.getTrack(trackIndex))
            for (int eventIndex = 0; eventIndex < sequence->getNumEvents(); ++eventIndex)
                if (const auto* event = sequence->getEventPointer(eventIndex);
                    event != nullptr && event->message.isTempoMetaEvent())
                {
                    const auto secondsPerQuarter = event->message.getTempoSecondsPerQuarterNote();
                    if (secondsPerQuarter > 1.0e-9)
                        importedBpm = 60.0 / secondsPerQuarter;
                    break;
                }
    midi.convertTimestampTicksToSeconds();
    std::vector<TrackData> importedTracks;
    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* sequence = midi.getTrack(trackIndex);
        if (sequence == nullptr) continue;
        juce::MidiMessageSequence matched(*sequence);
        matched.updateMatchedPairs();
        TrackData track;
        track.id = makeId("track");
        track.name = file.getFileNameWithoutExtension()
            + (midi.getNumTracks() > 1 ? " " + juce::String(trackIndex + 1) : juce::String());
        track.compose = true;
        ClipData clip;
        clip.id = makeId("clip");
        clip.sourceFile = file;
        clip.startSeconds = 0.0;
        for (int eventIndex = 0; eventIndex < matched.getNumEvents(); ++eventIndex)
        {
            const auto* event = matched.getEventPointer(eventIndex);
            if (event == nullptr || !event->message.isNoteOn()) continue;
            const auto offIndex = matched.getIndexOfMatchingKeyUp(eventIndex);
            const auto start = std::max(0.0, event->message.getTimeStamp());
            const auto end = offIndex >= 0
                ? std::max(start + 0.01, matched.getEventTime(offIndex)) : start + 0.25;
            NoteData note;
            note.id = makeId("note");
            note.startSeconds = start;
            note.durationSeconds = end - start;
            note.consonantSeconds = 0.0;
            note.midiNote = static_cast<float>(event->message.getNoteNumber());
            note.sourceMidiCenter = note.midiNote;
            note.contour.push_back({ 0.0, 0.0f, 0.0f, true });
            note.contour.push_back({ note.durationSeconds, 0.0f, 0.0f, true });
            clip.durationSeconds = std::max(clip.durationSeconds, end);
            clip.notes.push_back(std::move(note));
        }
        if (clip.notes.empty()) continue;
        clip.sourceDurationSeconds = clip.durationSeconds;
        track.clips.push_back(std::move(clip));
        importedTracks.push_back(std::move(track));
    }
    if (importedTracks.empty())
    {
        error = "MIDI file contains no notes: " + file.getFullPathName();
        return false;
    }
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        if (project.tracks.empty() && importedBpm)
            project.bpm = juce::jlimit(20.0, 400.0, *importedBpm);
        for (auto& track : importedTracks) project.tracks.push_back(std::move(track));
        if (project.name == "Untitled") project.name = file.getFileNameWithoutExtension();
    }
    sendChangeMessage();
    return true;
}

namespace
{
// A MIDI file as one-track import reads it.
//
// Positions are in quarter notes, the file's own musical time, so the notes
// can be put on the project's beats whatever its tempo.  A file timed in SMPTE
// frames has no beats: its positions are seconds, and say so.
struct MidiPart
{
    int index = 0;
    juce::String name;
    std::vector<NoteData> notes;   // start and duration in quarters, or seconds
};
struct MidiSong
{
    std::vector<MidiPart> parts;   // the tracks with notes, in file order
    std::optional<double> bpm;
    std::vector<TempoChange> tempoChanges;
    std::optional<std::pair<int, int>> meter;
    bool inSeconds = false;
};

juce::MemoryBlock metaEventBytes(const juce::MidiMessage& message)
{
    return juce::MemoryBlock(message.getMetaEventData(),
                             static_cast<size_t>(juce::jmax(0, message.getMetaEventLength())));
}

// A track's name and its lyrics, read in one encoding.  A MIDI file says
// nothing about how its text is written: a Japanese one is usually Shift-JIS,
// a Chinese one GBK, a newer one UTF-8.  Read together they are read the way
// a UST is -- valid UTF-8 as UTF-8, otherwise the reading with the most kana --
// and one lyric cannot come out in a different encoding from the next.
juce::StringArray decodeMidiTexts(const std::vector<juce::MemoryBlock>& texts)
{
    const auto clean = [](const juce::MemoryBlock& bytes)
    {
        juce::MemoryBlock kept;
        const auto* data = static_cast<const char*>(bytes.getData());
        for (size_t index = 0; index < bytes.getSize(); ++index)
            if (data[index] != '\n' && data[index] != '\r' && data[index] != 0)
                kept.append(data + index, 1);
        return kept;
    };
    juce::MemoryBlock joined;
    for (size_t index = 0; index < texts.size(); ++index)
    {
        if (index > 0) joined.append("\n", 1);
        const auto kept = clean(texts[index]);
        joined.append(kept.getData(), kept.getSize());
    }
    juce::String encoding;
    auto lines = juce::StringArray::fromTokens(backend::UstImporter::decode(joined, encoding),
                                               "\n", "");
    // No multi-byte encoding this can meet puts a line feed inside a
    // character, so the lines come back one for one; should they not, each is
    // read on its own rather than handed to the wrong note.
    if (lines.size() != static_cast<int>(texts.size()))
    {
        lines.clear();
        for (const auto& text : texts)
            lines.add(backend::UstImporter::decode(clean(text), encoding));
    }
    for (auto& line : lines) line = line.trim();
    return lines;
}

std::optional<MidiSong> readMidiSong(const juce::File& file, juce::String& error)
{
    auto input = file.createInputStream();
    if (input == nullptr)
    {
        error = "Could not open MIDI file: " + file.getFullPathName();
        return std::nullopt;
    }
    juce::MidiFile midi;
    if (!midi.readFrom(*input))
    {
        error = "Invalid MIDI file: " + file.getFullPathName();
        return std::nullopt;
    }
    MidiSong song;
    const auto ticksPerQuarter = static_cast<double>(midi.getTimeFormat());
    song.inSeconds = ticksPerQuarter <= 0.0;
    if (song.inSeconds) midi.convertTimestampTicksToSeconds();
    const auto positionOf = [&song, ticksPerQuarter](double stamp)
    {
        return juce::jmax(0.0, song.inSeconds ? stamp : stamp / ticksPerQuarter);
    };

    // The tempo map, from whichever tracks carry it: a type 1 file keeps it in
    // its conductor track, a type 0 file beside the notes.
    std::vector<std::pair<double, double>> speeds;
    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
        if (const auto* sequence = midi.getTrack(trackIndex))
            for (int eventIndex = 0; eventIndex < sequence->getNumEvents(); ++eventIndex)
            {
                const auto& message = sequence->getEventPointer(eventIndex)->message;
                if (message.isTempoMetaEvent() && message.getTempoSecondsPerQuarterNote() > 1.0e-9)
                    speeds.emplace_back(positionOf(message.getTimeStamp()),
                                        juce::jlimit(20.0, 400.0,
                                                     60.0 / message.getTempoSecondsPerQuarterNote()));
                else if (message.isTimeSignatureMetaEvent() && !song.meter)
                {
                    int numerator = 4, denominator = 4;
                    message.getTimeSignatureInfo(numerator, denominator);
                    song.meter = std::make_pair(numerator, denominator);
                }
            }
    std::stable_sort(speeds.begin(), speeds.end(),
                     [](const auto& left, const auto& right) { return left.first < right.first; });
    if (!song.inSeconds && !speeds.empty())
    {
        // Until the first change a MIDI file runs at 120.  Of two changes at
        // one place the later is the one that holds.
        song.bpm = speeds.front().first <= 1.0e-9 ? speeds.front().second : 120.0;
        for (std::size_t index = 0; index < speeds.size(); ++index)
        {
            if (index + 1 < speeds.size() && speeds[index + 1].first - speeds[index].first < 1.0e-9)
                continue;
            if (speeds[index].first <= 1.0e-9) song.bpm = speeds[index].second;
            else song.tempoChanges.push_back({ speeds[index].first, speeds[index].second });
        }
    }

    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* sequence = midi.getTrack(trackIndex);
        if (sequence == nullptr) continue;
        juce::MidiMessageSequence matched(*sequence);
        matched.updateMatchedPairs();
        MidiPart part;
        part.index = trackIndex;
        std::vector<juce::MemoryBlock> texts(1);   // the name first, then the lyrics
        auto named = false;
        std::optional<juce::MemoryBlock> pendingLyric;
        std::vector<std::pair<std::size_t, std::size_t>> lyricOf;   // note, text
        for (int eventIndex = 0; eventIndex < matched.getNumEvents(); ++eventIndex)
        {
            const auto& message = matched.getEventPointer(eventIndex)->message;
            if (message.isTrackNameEvent())
            {
                if (!named) texts.front() = metaEventBytes(message);
                named = true;
                continue;
            }
            // A lyric is sung on the note it comes before, or with: the next
            // one to start.
            if (message.isMetaEvent() && message.getMetaEventType() == 5)
            {
                pendingLyric = metaEventBytes(message);
                continue;
            }
            if (!message.isNoteOn()) continue;
            const auto offIndex = matched.getIndexOfMatchingKeyUp(eventIndex);
            const auto start = positionOf(message.getTimeStamp());
            const auto shortest = song.inSeconds ? 0.01 : 1.0 / 64.0;
            const auto end = offIndex >= 0
                ? juce::jmax(start + shortest, positionOf(matched.getEventTime(offIndex)))
                : start + (song.inSeconds ? 0.25 : 0.5);
            NoteData note;
            note.startSeconds = start;
            note.durationSeconds = end - start;
            note.consonantSeconds = 0.0;
            note.midiNote = static_cast<float>(message.getNoteNumber());
            note.sourceMidiCenter = note.midiNote;
            if (pendingLyric)
            {
                lyricOf.emplace_back(part.notes.size(), texts.size());
                texts.push_back(std::move(*pendingLyric));
                pendingLyric.reset();
            }
            part.notes.push_back(std::move(note));
        }
        if (part.notes.empty()) continue;
        const auto decoded = decodeMidiTexts(texts);
        part.name = decoded[0];
        for (const auto& [noteIndex, textIndex] : lyricOf)
            part.notes[noteIndex].label = decoded[static_cast<int>(textIndex)];
        song.parts.push_back(std::move(part));
    }
    if (song.parts.empty())
    {
        error = "MIDI file contains no notes: " + file.getFullPathName();
        return std::nullopt;
    }
    return song;
}
}

std::vector<ProjectModel::MidiTrackChoice> ProjectModel::midiTrackChoices(const juce::File& file,
                                                                          juce::String& error)
{
    std::vector<MidiTrackChoice> choices;
    if (const auto song = readMidiSong(file, error))
        for (const auto& part : song->parts)
            choices.push_back({ part.index, part.name, static_cast<int>(part.notes.size()) });
    return choices;
}

juce::String ProjectModel::addMidiTrack(const juce::File& file, int trackIndex,
                                        juce::String& error)
{
    auto song = readMidiSong(file, error);
    if (!song) return {};
    const auto part = std::find_if(song->parts.begin(), song->parts.end(),
                                   [trackIndex](const MidiPart& each) { return each.index == trackIndex; });
    if (part == song->parts.end())
    {
        error = "MIDI track " + juce::String(trackIndex + 1) + " has no notes: "
            + file.getFullPathName();
        return {};
    }

    TrackData track;
    track.id = makeId("track");
    track.name = part->name.isNotEmpty() ? part->name
        : file.getFileNameWithoutExtension()
              + (song->parts.size() > 1 ? " " + juce::String(trackIndex + 1) : juce::String());
    track.name = track.name.substring(0, 80);
    track.compose = true;
    ClipData clip;
    clip.id = makeId("clip");
    clip.sourceFile = file;
    clip.startSeconds = 0.0;
    clip.notes = std::move(part->notes);

    const auto id = track.id;
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        // The workflow the project is in, as a track 新建轨道 makes would.
        if (!project.tracks.empty())
        {
            track.pitchAlgorithm = project.tracks.back().pitchAlgorithm;
            track.stretchAlgorithm = project.tracks.back().stretchAlgorithm;
            track.renderOrder = project.tracks.back().renderOrder;
        }
        // The song's own tempo only when nothing is there yet to disagree
        // with; the tempo map has to be in place before a beat can become a
        // second.
        else if (!song->inSeconds)
        {
            if (song->bpm) project.bpm = *song->bpm;
            project.tempoChanges = song->tempoChanges;
            if (song->meter)
            {
                project.numerator = juce::jlimit(1, 32, song->meter->first);
                const auto denominator = song->meter->second;
                project.denominator = denominator == 2 || denominator == 8 || denominator == 16
                    ? denominator : 4;
            }
        }
        for (auto& note : clip.notes)
        {
            note.id = makeId("note");
            if (!song->inSeconds)
            {
                const auto startQuarters = note.startSeconds;
                const auto endQuarters = startQuarters + note.durationSeconds;
                note.startSeconds = project.secondsForQuarterPosition(startQuarters);
                note.durationSeconds = std::max(0.01,
                    project.secondsForQuarterPosition(endQuarters) - note.startSeconds);
            }
            note.contour.push_back({ 0.0, 0.0f, 0.0f, true });
            note.contour.push_back({ note.durationSeconds, 0.0f, 0.0f, true });
            clip.durationSeconds = std::max(clip.durationSeconds,
                                            note.startSeconds + note.durationSeconds);
        }
        std::stable_sort(clip.notes.begin(), clip.notes.end(),
            [](const auto& left, const auto& right) { return left.startSeconds < right.startSeconds; });
        clip.sourceDurationSeconds = clip.durationSeconds;
        track.clips.push_back(std::move(clip));
        project.tracks.push_back(std::move(track));
        if (project.name == "Untitled") project.name = file.getFileNameWithoutExtension();
    }
    sendChangeMessage();
    return id;
}

namespace
{
// A UST mode-2 pitch bend as this editor's pitch handles.
//
// Every pitch in a UST bend is in tenths of a semitone and is measured from
// the note's own NoteNum, so a note at 63 whose bend starts at -20 starts two
// semitones down, at 61 -- which is how a portamento out of the preceding
// note is written.  Times are milliseconds: PBS gives the first point's
// offset from the note's start (normally negative, so the bend begins inside
// the note before), and PBW gives the gap to each point after it.
//
// PBY is one short of PBW on purpose: the last point is the note's own pitch.
//
// PBM names the shape of each segment, and is indexed by the segment rather
// than the point -- entry i shapes the run from point i to point i+1, which is
// this editor's "shape of the incoming segment" on point i+1.
// A UST amplitude envelope as this editor's envelope handles.
//
// UTAU measures the shape from the beginning of the rendered output, which
// starts one preutterance before the note; this editor measures from the note
// itself, so time zero here is one preutterance later and the opening ramp
// sits at negative times.  That is the same convention envelopeForNoteAsItIs
// re-anchors at render time, and it is what keeps p1 lined up with the
// crossfade into the previous note.
//
// Volumes are percentages of the note's own level.  Zero is silence, and the
// renderer treats anything at or below -60 dB as silent, so that is the floor
// rather than an infinity.
void applyUstEnvelope(NoteData& note, const backend::UstNote& source,
                      double preutteranceSeconds)
{
    if (!source.hasEnvelope) return;
    const auto gainDb = [](double percent)
    {
        if (percent <= 0.01) return -60.0f;
        return juce::jlimit(-60.0f, 12.0f,
                            static_cast<float>(20.0 * std::log10(percent / 100.0)));
    };
    const auto fromStart = [preutteranceSeconds](double milliseconds)
    {
        return -preutteranceSeconds + milliseconds / 1000.0;
    };
    const auto fromEnd = [&note](double milliseconds)
    {
        return note.durationSeconds - milliseconds / 1000.0;
    };

    std::vector<AmplitudeEnvelopePoint> points;
    points.push_back({ fromStart(0.0), -60.0f });
    points.push_back({ fromStart(source.envelopeP1), gainDb(source.envelopeV1) });
    points.push_back({ fromStart(source.envelopeP1 + source.envelopeP2),
                       gainDb(source.envelopeV2) });
    if (source.hasMiddlePoint)
        points.push_back({ fromStart(source.envelopeP1 + source.envelopeP2
                                     + source.envelopeP5),
                           gainDb(source.envelopeV5) });
    points.push_back({ fromEnd(source.envelopeP3 + source.envelopeP4),
                       gainDb(source.envelopeV3) });
    points.push_back({ fromEnd(source.envelopeP3), gainDb(source.envelopeV4) });
    points.push_back({ fromEnd(0.0), -60.0f });

    // A short note can leave the opening ramp and the closing one overlapping,
    // and an envelope whose times run backwards is not one this editor can
    // draw or the renderer can read.  Push each point up to the one before it
    // rather than dropping any: the shape stays, squeezed.
    for (std::size_t index = 1; index < points.size(); ++index)
        points[index].timeSeconds = std::max(points[index].timeSeconds,
                                             points[index - 1].timeSeconds);
    for(auto& point:points)point.linearToNext=true;
    note.amplitudeEnvelope = std::move(points);
}

void applyUstPitchBend(NoteData& note, const backend::UstNote& source)
{
    if (!source.hasPitchBend) return;
    const auto pitchAt = [&note](double tenths)
    {
        return note.midiNote + static_cast<float>(tenths) * 0.1f;
    };
    const auto shapeFor = [&source](int segment)
    {
        const auto name = segment < source.shapes.size()
            ? source.shapes[segment] : juce::String();
        if (name == "s") return PitchCurveShape::linear;
        // UTAU's R rises fast and flattens; its J waits and then rises.
        if (name == "r") return PitchCurveShape::easeOut;
        if (name == "j") return PitchCurveShape::easeIn;
        // An empty entry is UTAU's default S-curve.
        return PitchCurveShape::smooth;
    };

    std::vector<PitchCurveEditPoint> points;
    points.reserve(source.widthsMs.size() + 1);
    auto time = source.pitchStartMs / 1000.0;
    points.push_back({ time, pitchAt(source.pitchStartTenths) });
    for (std::size_t index = 0; index < source.widthsMs.size(); ++index)
    {
        time += source.widthsMs[index] / 1000.0;
        const auto tenths = index < source.pitchTenths.size()
            ? source.pitchTenths[index] : 0.0;
        PitchCurveEditPoint point { time, pitchAt(tenths) };
        point.shape = shapeFor(static_cast<int>(index));
        points.push_back(point);
    }
    // A bend with one point says nothing the note's own pitch does not.
    if (points.size() < 2) return;
    note.pitchControlPoints = std::move(points);
}
}

bool ProjectModel::addUstFile(const juce::File& file, juce::String& error,
                              juce::StringArray& warnings, UstImportMode mode,
                              juce::String* importedTrackId, int encoding)
{
    const auto parsed = backend::UstImporter::read(file, error, warnings, encoding);
    if (!parsed) return false;

    TrackData track;
    track.id = makeId("track");
    track.name = parsed->name.isNotEmpty() ? parsed->name
                                           : file.getFileNameWithoutExtension();
    track.compose = true;
    // Plain UTAU: the four-region modes are this application's own, and a UST
    // says nothing that would fill them in.
    track.pitchAlgorithm = PitchAlgorithm::utau;
    track.utauMode = UtauMode::classic;
    track.utauGlobalFlags = parsed->globalFlags;
    track.ustSourceDocument=parsed->sourceText; track.ustSourceEncoding=parsed->sourceEncoding; track.ustSourceBom=parsed->sourceBom; track.ustSourceBytes=parsed->sourceBytes;

    ClipData clip;
    clip.id = makeId("clip");
    clip.sourceFile = file;
    clip.startSeconds = 0.0;

    // A UST places its notes by accumulating lengths in ticks, so the timeline
    // is built in musical time and turned into seconds through the project's
    // own tempo map afterwards.  Summing seconds as we went would drift the
    // moment a note carried a tempo change.
    std::vector<TempoChange> tempoChanges;
    std::vector<const backend::UstNote*> sources;
    auto quarters = 0.0;
    auto tempo = parsed->tempo;
    for (const auto& source : parsed->notes)
    {
        if (source.tempo && *source.tempo > 0.0 && *source.tempo != tempo)
        {
            tempo = *source.tempo;
            tempoChanges.push_back({ quarters, juce::jlimit(20.0, 400.0, tempo) });
        }
        const auto length = backend::UstImporter::quarterNotes(source.lengthTicks);
        // A rest becomes a gap rather than a silent note, which is the shape
        // the rest of this editor already works in.
        if (source.isRest() || length <= 0.0)
        {
            quarters += length;
            continue;
        }
        NoteData note;
        note.id = makeId("note");
        note.label = source.lyric.trim();
        note.nativeRole = note.label == "_" || note.label.containsChar('_')
            ? NativeSegmentRole::transition
            : note.label == "-" ? NativeSegmentRole::unknown
            : NativeSegmentRole::vowel;
            note.nativeProvenance = "utau";
            note.nativeConfidence = note.nativeRole == NativeSegmentRole::unknown ? 0.0f : 1.0f;
        note.midiNote = static_cast<float>(source.noteNum);
        note.sourceMidiCenter = note.midiNote;
        note.utauFlags = source.flags;
        note.ustSourceSection=source.sourceSection; note.ustSourceSectionIndex=source.sourceSectionIndex;
        note.utauModulationPercent=source.modulation.value_or(0);
        note.utauStpSeconds=source.stpMs.value_or(0)/1000.0;
        // A UST states every note's pitch, bend or no bend.  Without one the
        // note is on its own pitch throughout, and nothing of the editor's is
        // laid over either.
        note.utauAutoPitchTransition = false;
        // The vibrato as the file writes it; the editor's own vibrato is the
        // same seven numbers, so they are carried across as they stand.
        if (source.mode2 && source.hasVibrato)
        {
            note.vibratoEnabled = true;
            note.vibratoLengthPercent = juce::jlimit(0.0, 100.0, source.vibratoLengthPercent);
            note.vibratoCycleMs = std::max(1.0, source.vibratoCycleMs);
            note.vibratoDepthCents = source.vibratoDepthCents;
            note.vibratoFadeInPercent = juce::jlimit(0.0, 100.0, source.vibratoFadeInPercent);
            note.vibratoFadeOutPercent = juce::jlimit(0.0, 100.0, source.vibratoFadeOutPercent);
            note.vibratoPhasePercent = juce::jlimit(-100.0, 100.0, source.vibratoPhasePercent);
            note.vibratoOffsetPercent = juce::jlimit(-100.0, 100.0, source.vibratoOffsetPercent);
        }
        if (source.velocity) note.utauConsonantVelocity = *source.velocity;
        // UST intensity is a percentage; this editor keeps gain as a
        // multiplier and the renderer turns it back into a percentage.
        if (source.intensity)
            note.gain = juce::jlimit(0.0f, 2.0f,
                                     static_cast<float>(*source.intensity) / 100.0f);
        // An absent PreUtterance means "whatever the oto says", which is not
        // the same as an override of zero, so only a written value overrides.
        if (source.preutteranceMs)
        {
            note.utauPreutteranceOverrideEnabled = true;
            note.utauPreutteranceSeconds = *source.preutteranceMs / 1000.0;
        }
        if (source.overlapMs)
        {
            note.utauOverlapOverrideEnabled = true;
            note.utauOverlapSeconds = *source.overlapMs / 1000.0;
        }
        // Musical position for now; converted below, once the tempo map is in.
        note.startSeconds = quarters;
        note.durationSeconds = length;
        quarters += length;
        clip.notes.push_back(std::move(note));
        sources.push_back(&source);
    }

    if (clip.notes.empty())
    {
        error = "UST contains only rests: " + file.getFullPathName();
        return false;
    }

    const auto newTrackId = track.id;
    {
        const juce::ScopedLock guard(lock);
        if (mode == UstImportMode::replaceProject)
        {
            undoHistory.clear(); redoHistory.clear(); ++revision;
            project = ProjectData{};
        }
        else pushUndoLocked();
        // The tempo map has to be in place before a musical position can be
        // turned into seconds, and an imported song owns the tempo only when
        // it is the first thing in the project.
        if (project.tracks.empty())
        {
            project.bpm = juce::jlimit(20.0, 400.0, parsed->tempo);
            project.tempoChanges = tempoChanges;
        }
        else if (!tempoChanges.empty())
            warnings.add("tempo changes ignored: the project already has tracks");

        for (std::size_t index = 0; index < clip.notes.size(); ++index)
        {
            auto& note = clip.notes[index];
            const auto startQuarters = note.startSeconds;
            const auto endQuarters = startQuarters + note.durationSeconds;
            note.startSeconds = project.secondsForQuarterPosition(startQuarters);
            note.durationSeconds = std::max(0.01,
                project.secondsForQuarterPosition(endQuarters) - note.startSeconds);
            note.consonantSeconds = 0.0;
            note.nativeSegments = { { "segment_1", note.label.isEmpty() ? "-" : note.label,
                note.nativeRole, 0.0, note.durationSeconds, "utau",
                note.nativeConfidence, note.durationSeconds, note.utauOverlapSeconds,
                note.nativeRole != NativeSegmentRole::consonant, 1.0 } };
            note.contour.push_back({ 0.0, 0.0f, 0.0f, true });
            note.contour.push_back({ note.durationSeconds, 0.0f, 0.0f, true });
            if(parsed->mode2)applyUstPitchBend(note, *sources[index]);
            else backend::ustexchange::applyMode1(note,*sources[index],project.tempoAtSeconds(note.startSeconds),sources[index]>parsed->notes.data()?sources[index]-1:nullptr);
            // The envelope is drawn against the preutterance the UST named.
            // Where it named none the oto's own is not known here -- no
            // voicebank has been chosen yet -- so the shape is anchored at the
            // note and envelopeForNoteAsItIs moves it, ramps intact, once the
            // real preutterance is known at render time.
            applyUstEnvelope(note, *sources[index],
                             note.utauPreutteranceOverrideEnabled
                                 ? note.utauPreutteranceSeconds : 0.0);
            note.ustBaseline=juce::JSON::toString(backend::ustexchange::noteState(note,project,clip.startSeconds,parsed->mode2),true);
        }
        clip.durationSeconds = std::max(project.secondsForQuarterPosition(quarters),
            clip.notes.back().startSeconds + clip.notes.back().durationSeconds);
        clip.sourceDurationSeconds = clip.durationSeconds;
        track.clips.push_back(std::move(clip));
        auto originalTempoProject=project;originalTempoProject.bpm=parsed->tempo;originalTempoProject.tempoChanges=tempoChanges;
        auto ustTrackBaseline=backend::ustexchange::trackState(track,project);
        const auto originalTempoState=backend::ustexchange::trackState(track,originalTempoProject);
        ustTrackBaseline.getDynamicObject()->setProperty("Tempo",originalTempoState["Tempo"]);
        ustTrackBaseline.getDynamicObject()->setProperty("_tempos",originalTempoState["_tempos"]);
        track.ustBaseline=juce::JSON::toString(ustTrackBaseline,true);
        project.tracks.push_back(std::move(track));
        if (project.name == "Untitled" && parsed->name.isNotEmpty())
            project.name = parsed->name;
    }
    if (importedTrackId != nullptr) *importedTrackId = newTrackId;
    if (parsed->voiceDirectory.isNotEmpty())
        warnings.add("this UST asks for voicebank \"" + parsed->voiceDirectory
                     + "\" -- choose it under Settings / Algorithm");
    sendChangeMessage();
    return true;
}

bool ProjectModel::exportUst(const juce::File& file,const juce::String& trackId,juce::String& error,juce::StringArray& warnings,int encoding) const
{ return backend::ustexchange::write(snapshot(),trackId,file,error,warnings,encoding); }

void ProjectModel::setTempo(double bpm, int numerator, int denominator)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        project.bpm = juce::jlimit(20.0, 400.0, bpm);
        project.numerator = juce::jlimit(1, 32, numerator);
        project.denominator = denominator == 2 || denominator == 8 || denominator == 16
            ? denominator : 4;
    }
    sendChangeMessage();
}

void ProjectModel::setTempoChange(double quarterPosition, double bpm, bool synchronizeNotes)
{
    const auto position = std::max(0.0, quarterPosition);
    const auto tempo = juce::jlimit(20.0, 400.0, bpm);
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        const auto before = project;
        auto existing = std::find_if(project.tempoChanges.begin(),
            project.tempoChanges.end(), [&](const auto& change)
            {
                return std::abs(change.quarterPosition - position) < 1.0e-7;
            });
        if (position <= 1.0e-7)
        {
            if (std::abs(project.bpm - tempo) < 1.0e-7) return;
        }
        else if (existing != project.tempoChanges.end()
                 && std::abs(existing->bpm - tempo) < 1.0e-7)
            return;

        pushUndoLocked();
        if (position <= 1.0e-7)
            project.bpm = tempo;
        else if (existing != project.tempoChanges.end())
            existing->bpm = tempo;
        else
            project.tempoChanges.push_back({ position, tempo });
        std::stable_sort(project.tempoChanges.begin(), project.tempoChanges.end(),
            [](const auto& left, const auto& right)
            {
                return left.quarterPosition < right.quarterPosition;
            });

        const auto remapTime = [&](double oldSeconds)
        {
            return project.secondsForQuarterPosition(
                before.quarterPositionForSeconds(oldSeconds));
        };
        for (std::size_t trackIndex = 0; trackIndex < project.tracks.size(); ++trackIndex)
        {
            auto& track = project.tracks[trackIndex];
            if (!synchronizeNotes || !track.compose || trackIndex >= before.tracks.size()) continue;
            const auto& oldTrack = before.tracks[trackIndex];
            for (std::size_t clipIndex = 0;
                 clipIndex < track.clips.size() && clipIndex < oldTrack.clips.size();
                 ++clipIndex)
            {
                auto& clip = track.clips[clipIndex];
                const auto& oldClip = oldTrack.clips[clipIndex];
                const auto oldClipStart = oldClip.startSeconds;
                const auto oldClipEnd = oldClipStart + oldClip.durationSeconds;
                const auto newClipStart = remapTime(oldClipStart);
                const auto newClipEnd = remapTime(oldClipEnd);
                clip.startSeconds = newClipStart;
                clip.durationSeconds = std::max(0.01, newClipEnd - newClipStart);

                const auto remapClipOffset = [&](double oldOffset)
                {
                    return remapTime(oldClipStart + oldOffset) - newClipStart;
                };
                for (auto& point : clip.sourceTimeMap)
                    point.targetSeconds = remapClipOffset(point.targetSeconds);
                for (auto& point : clip.nativeTrimClock)
                    point.targetSeconds = remapClipOffset(point.targetSeconds);
                const auto oldAudioStart = oldClipStart + oldClip.audioStartSeconds;
                const auto oldAudioEnd = oldAudioStart + oldClip.audioLength();
                const auto newAudioStart = remapTime(oldAudioStart);
                const auto newAudioEnd = remapTime(oldAudioEnd);
                if (oldClip.audioDurationSeconds >= 0.0)
                {
                    clip.audioStartSeconds = newAudioStart - newClipStart;
                    clip.audioDurationSeconds = std::max(0.0, newAudioEnd - newAudioStart);
                }
                clip.fadeInSeconds = juce::jlimit(0.0, clip.audioLength(),
                    remapTime(oldAudioStart + oldClip.fadeInSeconds) - newAudioStart);
                clip.fadeOutSeconds = juce::jlimit(0.0, clip.audioLength(),
                    newAudioEnd - remapTime(oldAudioEnd - oldClip.fadeOutSeconds));

                for(auto& part:clip.parts)
                {
                    const auto oldStart=oldClipStart+part.startSeconds;
                    const auto oldEnd=oldStart+part.durationSeconds;
                    const auto newStart=remapTime(oldStart);
                    const auto audioStart=oldStart+part.audioStartSeconds;
                    const auto audioEnd=audioStart+part.audioLength();
                    part.audioDurationSeconds=remapTime(audioEnd)-remapTime(audioStart);
                    part.audioStartSeconds=remapTime(audioStart)-newStart;
                    part.fadeInSeconds=remapTime(audioStart+part.fadeInSeconds)-remapTime(audioStart);
                    part.fadeOutSeconds=remapTime(audioEnd)-remapTime(audioEnd-part.fadeOutSeconds);
                    for(auto& point:part.sourceTimeMap)point.targetSeconds=remapTime(oldStart+point.targetSeconds)-newStart;
                    for(auto& point:part.nativeTrimClock)point.targetSeconds=remapTime(oldStart+point.targetSeconds)-newStart;
                    part.startSeconds=newStart-newClipStart;part.durationSeconds=remapTime(oldEnd)-newStart;
                }

                for (std::size_t noteIndex = 0;
                     noteIndex < clip.notes.size() && noteIndex < oldClip.notes.size();
                     ++noteIndex)
                {
                    auto& note = clip.notes[noteIndex];
                    const auto& oldNote = oldClip.notes[noteIndex];
                    const auto oldNoteStart = oldClipStart + oldNote.startSeconds;
                    const auto oldNoteEnd = oldNoteStart + oldNote.durationSeconds;
                    const auto newNoteStart = remapTime(oldNoteStart);
                    const auto newNoteEnd = remapTime(oldNoteEnd);
                    note.startSeconds = newNoteStart - newClipStart;
                    note.durationSeconds = std::max(0.001,
                                                    newNoteEnd - newNoteStart);
                    note.consonantSeconds = juce::jlimit(0.0, note.durationSeconds,
                        remapTime(oldNoteStart + oldNote.consonantSeconds)
                            - newNoteStart);
                    for (std::size_t index = 0;
                         index < note.contour.size() && index < oldNote.contour.size(); ++index)
                        note.contour[index].timeSeconds = remapTime(oldNoteStart
                            + oldNote.contour[index].timeSeconds) - newNoteStart;
                    for (std::size_t index = 0;
                         index < note.pitchControlPoints.size()
                            && index < oldNote.pitchControlPoints.size(); ++index)
                        note.pitchControlPoints[index].timeSeconds = remapTime(oldNoteStart
                            + oldNote.pitchControlPoints[index].timeSeconds) - newNoteStart;
                    for (std::size_t index = 0;
                         index < note.diffSingerPitchReference.size()
                            && index < oldNote.diffSingerPitchReference.size(); ++index)
                        note.diffSingerPitchReference[index].timeSeconds = remapTime(oldNoteStart
                            + oldNote.diffSingerPitchReference[index].timeSeconds) - newNoteStart;
                    for (std::size_t index = 0;
                         index < note.diffSingerPitchOffset.size()
                            && index < oldNote.diffSingerPitchOffset.size(); ++index)
                        note.diffSingerPitchOffset[index].timeSeconds = remapTime(oldNoteStart
                            + oldNote.diffSingerPitchOffset[index].timeSeconds) - newNoteStart;
                    mapDiffSingerParameterTimes(note,[&](double time){return remapTime(oldNoteStart+time)-newNoteStart;});
                    for (std::size_t index = 0;
                         index < note.amplitudeEnvelope.size()
                            && index < oldNote.amplitudeEnvelope.size(); ++index)
                        note.amplitudeEnvelope[index].timeSeconds = remapTime(oldNoteStart
                            + oldNote.amplitudeEnvelope[index].timeSeconds) - newNoteStart;
                    for (std::size_t index = 0;
                         index < note.sibilantMarkers.size()
                            && index < oldNote.sibilantMarkers.size(); ++index)
                        note.sibilantMarkers[index] = remapTime(oldNoteStart
                            + oldNote.sibilantMarkers[index]) - newNoteStart;
                    clip.durationSeconds = std::max(clip.durationSeconds,
                        note.startSeconds + note.durationSeconds);
                }
            }
        }
        changed = true;
    }
    if (changed) sendChangeMessage();
}

juce::int64 ProjectModel::contentFingerprint() const
{
    juce::MemoryOutputStream stream;
    toValueTree(juce::File()).writeToStream(stream);
    // Hash the raw bytes.  A serialised ValueTree is binary and full of zero
    // bytes, so reading it back as a string would stop at the first one and
    // leave almost the whole project out of the digest.
    const auto* bytes = static_cast<const juce::uint8*>(stream.getData());
    auto hash = 1469598103934665603ull;
    for (std::size_t index = 0; index < stream.getDataSize(); ++index)
    {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return static_cast<juce::int64>(hash);
}

void ProjectModel::setGridDivision(const juce::String& division)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        project.gridDivision = division;
    }
    sendChangeMessage();
}

void ProjectModel::setNoteEditDivision(int division)
{
    const auto value = juce::jlimit(2, 128, division);
    {
        const juce::ScopedLock guard(lock);
        if (project.noteEditDivision == value) return;
        pushUndoLocked();
        project.noteEditDivision = value;
    }
    sendChangeMessage();
}

void ProjectModel::setBaseScale(const juce::String& scale)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        project.baseScale = scale;
    }
    sendChangeMessage();
}

void ProjectModel::setTrackCompose(const juce::String& trackId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.compose != enabled)
            {
                pushUndoLocked(); track.compose = enabled; changed = true; break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackMuted(const juce::String& trackId, bool muted)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        for (auto& track : project.tracks)
            if (track.id == trackId)
                track.muted = muted;
    }
    sendChangeMessage();
}

void ProjectModel::setTrackSolo(const juce::String& trackId, bool solo)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        for (auto& track : project.tracks)
            if (track.id == trackId)
                track.solo = solo;
    }
    sendChangeMessage();
}

void ProjectModel::setTrackVolume(const juce::String& trackId, float volume)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        for (auto& track : project.tracks)
            if (track.id == trackId)
                track.volume = juce::jlimit(0.0f, 2.0f, volume);
    }
    sendChangeMessage();
}

void ProjectModel::setClipGainEnvelope(const juce::String& clipId, std::vector<GainEnvelopePoint> points)
{
    points = normaliseTrackGainEnvelope(std::move(points));
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks) for (auto& clip : track.clips)
            if (clip.id == clipId)
            {
                if (sameTrackGainEnvelope(clip.gainEnvelope, points)) return;
                pushUndoLocked();
                clip.gainEnvelope = std::move(points);
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackPan(const juce::String& trackId, float pan)
{
    {
        const juce::ScopedLock guard(lock);
        pushUndoLocked();
        for (auto& track : project.tracks)
            if (track.id == trackId)
                track.pan = juce::jlimit(-1.0f, 1.0f, pan);
    }
    sendChangeMessage();
}

void ProjectModel::setTrackSmoothOverlaps(const juce::String& trackId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.smoothOverlaps != enabled)
            {
                pushUndoLocked();
                track.smoothOverlaps = enabled;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackNormalizeVolume(const juce::String& trackId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.normalizeVolume != enabled)
            {
                pushUndoLocked();
                track.normalizeVolume = enabled;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackNsfSmoothPitchTransitions(const juce::String& trackId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.nsfSmoothPitchTransitions != enabled)
            {
                pushUndoLocked();
                track.nsfSmoothPitchTransitions = enabled;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackNsfNoiseProtection(const juce::String& trackId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.nsfNoiseProtection != enabled)
            {
                pushUndoLocked();
                track.nsfNoiseProtection = enabled;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackVoicebankDirectory(const juce::String& trackId,
                                               const juce::File& directory)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.voicebankDirectory != directory)
            {
                pushUndoLocked();
                const auto wasDiffSinger = trackIsDiffSinger(track);
                track.voicebankDirectory = directory;
                if (directory.getChildFile("dsconfig.yaml").existsAsFile()) track.utauMode = UtauMode::mou;
                if (!wasDiffSinger && trackIsDiffSinger(track))
                    for (auto& clip : track.clips) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackUtauConsonantVelocity(const juce::String& trackId,
                                                  int velocity)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.utauConsonantVelocity != velocity)
            {
                pushUndoLocked();
                track.utauConsonantVelocity = velocity;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackUtauGlobalFlags(const juce::String& trackId,
                                            const juce::String& flags)
{
    const auto normalized = flags.trim();
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.utauGlobalFlags != normalized)
            {
                pushUndoLocked();
                track.utauGlobalFlags = normalized;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackChineseCvvc(const juce::String& trackId, bool enabled)
{
    bool changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.chineseCvvc != enabled)
            {
                pushUndoLocked();
                track.chineseCvvc = enabled;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setPitchAlgorithm(PitchAlgorithm algorithm)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (!track.accompaniment && (track.pitchAlgorithm != algorithm
                || (algorithm == PitchAlgorithm::nsfHifigan && !track.nativeNsfAudio)))
            {
                if (!changed) pushUndoLocked();
                const auto wasDiffSinger = trackIsDiffSinger(track);
                track.pitchAlgorithm = algorithm;
                if (algorithm == PitchAlgorithm::nsfHifigan) track.nativeNsfAudio = true;
                if (!wasDiffSinger && trackIsDiffSinger(track))
                    for (auto& clip : track.clips) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
                changed = true;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setUtauMode(UtauMode mode)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (!track.accompaniment && track.utauMode != mode)
            {
                if (!changed) pushUndoLocked();
                const auto wasDiffSinger = trackIsDiffSinger(track);
                track.utauMode = track.voicebankDirectory.getChildFile("dsconfig.yaml").existsAsFile()
                    ? UtauMode::mou : mode;
                if (!wasDiffSinger && trackIsDiffSinger(track))
                    for (auto& clip : track.clips) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
                changed = true;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackOutputEngine(const juce::String& trackId, UtauOutputEngine engine,
    const juce::File& resampler, const juce::File& wavtool)
{
    bool changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && trackUsesVoicebankSynthesis(track) && !trackIsDiffSinger(track))
            {
                if (track.outputEngine == engine && track.outputResampler == resampler && track.outputWavtool == wavtool) break;
                pushUndoLocked();
                track.outputEngine = engine;
                track.outputResampler = resampler;
                track.outputWavtool = wavtool;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackUtauMode(const juce::String& trackId, UtauMode mode)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.utauMode != mode)
            {
                pushUndoLocked();
                const auto wasDiffSinger = trackIsDiffSinger(track);
                track.utauMode = track.voicebankDirectory.getChildFile("dsconfig.yaml").existsAsFile()
                    ? UtauMode::mou : mode;
                if (!wasDiffSinger && trackIsDiffSinger(track))
                    for (auto& clip : track.clips) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackPitchAlgorithm(const juce::String& trackId,
                                          PitchAlgorithm algorithm)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && (track.pitchAlgorithm != algorithm
                || (algorithm == PitchAlgorithm::nsfHifigan && !track.nativeNsfAudio)))
            {
                pushUndoLocked();
                const auto wasDiffSinger = trackIsDiffSinger(track);
                track.pitchAlgorithm = algorithm;
                if (algorithm == PitchAlgorithm::nsfHifigan) track.nativeNsfAudio = true;
                if (!wasDiffSinger && trackIsDiffSinger(track))
                    for (auto& clip : track.clips) for (auto& note : clip.notes) note.utauFlagCurveEnabled = true;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setStretchAlgorithm(StretchAlgorithm algorithm)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (!track.accompaniment && track.stretchAlgorithm != algorithm)
            {
                if (!changed) pushUndoLocked();
                track.stretchAlgorithm = algorithm;
                changed = true;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackStretchAlgorithm(const juce::String& trackId,
                                            StretchAlgorithm algorithm)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.stretchAlgorithm != algorithm)
            {
                pushUndoLocked();
                track.stretchAlgorithm = algorithm;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setRenderOrder(RenderOrder order)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (!track.accompaniment && track.renderOrder != order)
            {
                if (!changed) pushUndoLocked();
                track.renderOrder = order;
                changed = true;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setTrackRenderOrder(const juce::String& trackId, RenderOrder order)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && !track.accompaniment && track.renderOrder != order)
            {
                pushUndoLocked();
                track.renderOrder = order;
                changed = true;
                break;
            }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::moveClip(const juce::String& clipId,double startSeconds)
{
    const auto data=snapshot();for(const auto& track:data.tracks)for(const auto& clip:track.clips)
        if(clip.id==clipId){moveClips({clipId},std::max(0.0,startSeconds)-clip.startSeconds);return;}
}

std::optional<double> ProjectModel::clipSplitPositionLocked(
    const juce::String& clipId, double absoluteSeconds) const
{
    if (!std::isfinite(absoluteSeconds)) return {};
    for (const auto& track : project.tracks)
        for (const auto& clip : track.clips)
            if (clip.id == clipId)
            {
                const auto cut = absoluteSeconds - clip.startSeconds;
                if (cut < 0.01 || cut > clip.durationSeconds - 0.01) return {};
                if (track.accompaniment || !track.compose || clip.notes.empty())
                    return canSplitClipAt(clipId, absoluteSeconds)
                        ? std::optional<double>(absoluteSeconds) : std::nullopt;
                std::vector<std::pair<double, double>> spans;
                spans.reserve(clip.notes.size());
                for (const auto& note : clip.notes)
                {
                    const auto end = note.startSeconds + note.durationSeconds;
                    if (!std::isfinite(note.startSeconds) || !std::isfinite(end)
                        || note.durationSeconds <= 0.0) return {};
                    spans.emplace_back(note.startSeconds, end);
                }
                std::sort(spans.begin(), spans.end());
                // Keep overlapping notes together: the previous note alone
                // cannot tell us whether a longer note still crosses the gap.
                auto reach = spans.front().second;
                std::optional<double> best;
                auto distance = std::numeric_limits<double>::infinity();
                for (std::size_t i = 1; i < spans.size(); ++i)
                {
                    const auto low = std::max(0.01, reach);
                    const auto high = std::min(clip.durationSeconds - 0.01, spans[i].first);
                    if (low <= high + 1.0e-9)
                    {
                        // Treat sub-nanosecond rounding at touching note boundaries as adjacency.
                        const auto candidate = low <= high ? juce::jlimit(low, high, cut) : high;
                        const auto delta = std::abs(candidate - cut);
                        // At equal distance prefer the earlier boundary.
                        if (delta < distance - 1.0e-9)
                        {
                            best = clip.startSeconds + candidate;
                            distance = delta;
                        }
                    }
                    reach = std::max(reach, spans[i].second);
                }
                return best && canSplitClipAt(clipId, *best) ? best : std::nullopt;
            }
    return {};
}

bool ProjectModel::canSplitClip(const juce::String& clipId, double absoluteSeconds) const
{
    const juce::ScopedLock guard(lock);
    return clipSplitPositionLocked(clipId, absoluteSeconds).has_value();
}

juce::String ProjectModel::splitClip(const juce::String& clipId, double absoluteSeconds)
{
    const juce::ScopedLock guard(lock);
    const auto position = clipSplitPositionLocked(clipId, absoluteSeconds);
    return position ? splitClipAt(clipId, *position) : juce::String{};
}

bool ProjectModel::canSplitClipAt(const juce::String& clipId, double absoluteSeconds) const
{
    if (!std::isfinite(absoluteSeconds)) return false;
    const juce::ScopedLock guard(lock);
    for (const auto& track : project.tracks)
        for (const auto& clip : track.clips)
            if (clip.id == clipId)
            {
                const auto cut = absoluteSeconds - clip.startSeconds;
                if (cut < 0.01 || cut > clip.durationSeconds - 0.01) return false;
                for (const auto& note : clip.notes)
                {
                    const auto inside = cut - note.startSeconds;
                    if (inside > 1.0e-8 && inside < note.durationSeconds - 1.0e-8
                        && (inside < 0.01 || note.durationSeconds - inside < 0.01)) return false;
                }
                return true;
            }
    return false;
}

juce::String ProjectModel::splitClipAt(const juce::String& clipId, double absoluteSeconds)
{
    juce::String rightId;
    {
        const juce::ScopedLock guard(lock);
        if (!canSplitClipAt(clipId, absoluteSeconds)) return {};
        for (auto& track : project.tracks)
        {
            const auto found = std::find_if(track.clips.begin(), track.clips.end(),
                [&](const auto& clip) { return clip.id == clipId; });
            if (found == track.clips.end()) continue;
            const auto index = static_cast<std::size_t>(found - track.clips.begin());
            auto original = *found;
            if(trackShowsAllNativeRegions(track))rememberNativeTrimSources(original);
            const auto cut = absoluteSeconds - original.startSeconds;
            const auto audioLength = original.audioLength();
            const auto audioCut = juce::jlimit(0.0, audioLength, cut - original.audioStartSeconds);
            const auto sourceDuration = track.accompaniment ? audioLength
                : (original.sourceDurationSeconds > 1.0e-9 ? original.sourceDurationSeconds : audioLength);
            auto sourceCut = audioLength > 1.0e-9 ? audioCut * sourceDuration / audioLength : 0.0;
            const auto useSourceTimeMap = !track.accompaniment && track.compose
                && !original.notes.empty() && original.sourceTimeMap.size() >= 2;
            if (useSourceTimeMap && audioCut > 0.0 && audioCut < audioLength)
            {
                const auto next = std::upper_bound(original.sourceTimeMap.begin(), original.sourceTimeMap.end(), cut,
                    [](double time, const SourceTimePoint& p) { return time < p.targetSeconds; });
                if (next == original.sourceTimeMap.begin()) sourceCut = next->sourceSeconds;
                else if (next == original.sourceTimeMap.end()) sourceCut = original.sourceTimeMap.back().sourceSeconds;
                else
                {
                    const auto& before = *(next - 1);
                    const auto span = next->targetSeconds - before.targetSeconds;
                    sourceCut = before.sourceSeconds + (next->sourceSeconds - before.sourceSeconds)
                        * (span > 1.0e-9 ? (cut - before.targetSeconds) / span : 0.0);
                }
                sourceCut = juce::jlimit(0.0, sourceDuration, sourceCut);
            }
            pushUndoLocked();
            const juce::ScopedValueSetter<bool> grouping(suppressNestedUndo, true);
            for (const auto& note : original.notes)
            {
                const auto inside = cut - note.startSeconds;
                if (inside > 1.0e-8 && inside < note.durationSeconds - 1.0e-8)
                {
                    const auto newNote = splitNote(note.id, inside);
                    // Classic linear flags are note-local too. The note splitter
                    // already handles DS predictions and offsets separately.
                    for (auto& part : found->notes)
                        if (part.id == newNote)
                            for (auto& curve : part.utauFlagCurves)
                                if (!isDiffSingerParameter(curve.flag))
                                    for (auto& p : curve.points) p.timeSeconds -= inside;
                }
            }
            auto left = *found, right = *found;
            if(trackShowsAllNativeRegions(track))
            {rememberNativeTrimSources(left);right.nativeTrimClock=left.nativeTrimClock;right.nativeSourcePitch=left.nativeSourcePitch;right.nativeSourcePitchComplete=left.nativeSourcePitchComplete;}
            if (!original.parts.empty())
            {
                left.parts = slicedClipParts(original.parts,0,cut,track.accompaniment,track.compose);
                right.parts = slicedClipParts(original.parts,cut,original.durationSeconds,track.accompaniment,track.compose);
            }
            right.id = rightId = makeId("clip");
            left.durationSeconds = cut;
            right.startSeconds = absoluteSeconds;
            right.durationSeconds = original.durationSeconds - cut;
            for(auto& point:right.nativeTrimClock)point.targetSeconds-=cut;
            shiftClipGainEnvelopes(right,-cut);
            anchorClipGainEnvelope(left);anchorClipGainEnvelope(right);
            left.audioStartSeconds = std::min(original.audioStartSeconds, cut);
            left.audioDurationSeconds = audioCut;
            right.audioStartSeconds = std::max(0.0, original.audioStartSeconds - cut);
            right.audioDurationSeconds = audioLength - audioCut;
            left.sourceDurationSeconds = sourceCut;
            right.sourceOffsetSeconds = original.sourceOffsetSeconds + sourceCut;
            right.sourceDurationSeconds = sourceDuration - sourceCut;
            if (!original.parts.empty())
            {
                if (left.parts.empty()) left.audioDurationSeconds = left.sourceDurationSeconds = 0;
                if (right.parts.empty()) right.audioDurationSeconds = right.sourceDurationSeconds = 0;
            }
            left.sourceTimeMap.clear(); right.sourceTimeMap.clear();
            if (useSourceTimeMap)
            {
                if (left.audioDurationSeconds > 0.0)
                    left.sourceTimeMap.push_back({left.audioStartSeconds, 0.0});
                for (const auto& p : original.sourceTimeMap)
                {
                    if (p.targetSeconds > original.audioStartSeconds && p.targetSeconds < cut
                        && p.targetSeconds < original.audioStartSeconds + audioLength) left.sourceTimeMap.push_back(p);
                    if (p.targetSeconds > cut && p.targetSeconds > original.audioStartSeconds
                        && p.targetSeconds < original.audioStartSeconds + audioLength)
                        right.sourceTimeMap.push_back({p.targetSeconds - cut, p.sourceSeconds - sourceCut});
                }
                if (left.audioDurationSeconds > 0.0)
                    left.sourceTimeMap.push_back({left.audioStartSeconds + left.audioDurationSeconds, sourceCut});
                if (right.audioDurationSeconds > 0.0)
                {
                    right.sourceTimeMap.insert(right.sourceTimeMap.begin(), {right.audioStartSeconds, 0.0});
                    right.sourceTimeMap.push_back({right.audioStartSeconds + right.audioDurationSeconds, right.sourceDurationSeconds});
                }
            }
            std::erase_if(left.notes, [&](const auto& note) { return note.startSeconds >= cut - 1.0e-8; });
            std::erase_if(right.notes, [&](const auto& note) { return note.startSeconds < cut - 1.0e-8; });
            for (auto& note : right.notes) note.startSeconds -= cut;
            left.fadeInSeconds = std::min(left.durationSeconds, original.fadeInSeconds);
            right.fadeOutSeconds = std::min(right.durationSeconds, original.fadeOutSeconds);
            left.crossfadeInSeconds = std::min(left.durationSeconds, original.crossfadeInSeconds);
            right.crossfadeOutSeconds = std::min(right.durationSeconds, original.crossfadeOutSeconds);
            left.fadeOutSeconds = audioCut >= audioLength ? original.fadeOutSeconds : 0.0;
            left.crossfadeOutSeconds = audioCut >= audioLength ? original.crossfadeOutSeconds : 0.0;
            right.fadeInSeconds = audioCut <= 0.0 ? original.fadeInSeconds : 0.0;
            right.crossfadeInSeconds = audioCut <= 0.0 ? original.crossfadeInSeconds : 0.0;
            left.glideConnectedToNext = right.glideConnectedFromPrevious = false;
            for(auto* piece:{&left,&right})
                if(piece->parts.size()==1)
                {
                    auto plain=expandedClipParts(*piece,true).front();
                    for(auto& note:plain.notes)note.clipPartId.clear();
                    *piece=std::move(plain);
                }
            track.clips[index] = std::move(left);
            track.clips.insert(track.clips.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));
            break;
        }
    }
    if (rightId.isNotEmpty()) sendChangeMessage();
    return rightId;
}

bool ProjectModel::canMergeClips(const std::vector<juce::String>& clipIds) const
{
    const juce::ScopedLock guard(lock);
    std::vector<juce::String> unique;
    for (const auto& id : clipIds) if (std::find(unique.begin(),unique.end(),id)==unique.end()) unique.push_back(id);
    if (unique.size()<2) return false;
    const TrackData* owner=nullptr;std::size_t found=0;
    for(const auto& track:project.tracks)for(const auto& clip:track.clips)
        if(std::find(unique.begin(),unique.end(),clip.id)!=unique.end())
        { if(owner && owner!=&track)return false;owner=&track;++found; }
    return found==unique.size();
}

juce::String ProjectModel::mergeClips(const std::vector<juce::String>& clipIds)
{
    juce::String mergedId;
    {
        const juce::ScopedLock guard(lock);
        if(!canMergeClips(clipIds))return {};
        for(auto& track:project.tracks)
        {
            std::vector<ClipData> chosen;
            for(const auto& clip:track.clips)
                if(std::find(clipIds.begin(),clipIds.end(),clip.id)!=clipIds.end())chosen.push_back(clip);
            if(chosen.empty())continue;
            std::stable_sort(chosen.begin(),chosen.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
            ClipData merged;merged.id=mergedId=chosen.front().id;merged.startSeconds=chosen.front().startSeconds;
            merged.sourceFile=chosen.front().sourceFile;merged.durationSeconds=0;
            for(const auto& clip:chosen)
            {
                merged.showNoteHints = merged.showNoteHints || clip.showNoteHints;
                merged.showNormalDisplay = merged.showNormalDisplay || clip.showNormalDisplay;
                merged.durationSeconds=std::max(merged.durationSeconds,clip.startSeconds+clip.durationSeconds-merged.startSeconds);
                for(auto part:expandedClipParts(clip))
                {
                    const auto oldStart=part.startSeconds;
                    part.id=makeId("part");part.startSeconds=oldStart-merged.startSeconds;
                    for(auto note:part.notes)
                    { note.startSeconds+=part.startSeconds;note.clipPartId=part.id;merged.notes.push_back(std::move(note)); }
                    part.notes.clear();merged.parts.push_back(std::move(part));
                }
            }
            std::stable_sort(merged.notes.begin(),merged.notes.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
            pushUndoLocked();
            const auto insertAt=static_cast<std::size_t>(std::find_if(track.clips.begin(),track.clips.end(),
                [&](const auto& c){return std::find(clipIds.begin(),clipIds.end(),c.id)!=clipIds.end();})-track.clips.begin());
            std::erase_if(track.clips,[&](const auto& c){return std::find(clipIds.begin(),clipIds.end(),c.id)!=clipIds.end();});
            track.clips.insert(track.clips.begin()+static_cast<std::ptrdiff_t>(insertAt),std::move(merged));
            break;
        }
    }
    if(mergedId.isNotEmpty())sendChangeMessage();
    return mergedId;
}

juce::String ProjectModel::duplicateClip(const juce::String& clipId,
                                         double startSeconds,
                                         const juce::String& targetTrackId)
{
    juce::String insertedId;
    {
        const juce::ScopedLock guard(lock);
        auto sourceTrackIndex = project.tracks.size();
        ClipData copy;
        auto found = false;
        for (std::size_t trackIndex = 0; trackIndex < project.tracks.size() && !found;
             ++trackIndex)
            for (const auto& clip : project.tracks[trackIndex].clips)
                if (clip.id == clipId)
                {
                    sourceTrackIndex = trackIndex;
                    copy = clip;
                    found = true;
                    break;
                }
        if (!found || sourceTrackIndex >= project.tracks.size()) return {};

        auto destinationTrackIndex = sourceTrackIndex;
        if (targetTrackId.isNotEmpty())
            for (std::size_t trackIndex = 0; trackIndex < project.tracks.size(); ++trackIndex)
                if (project.tracks[trackIndex].id == targetTrackId)
                {
                    destinationTrackIndex = trackIndex;
                    break;
                }

        pushUndoLocked();
        copy.id = makeId("clip");
        copy.startSeconds = startSeconds >= 0.0
            ? startSeconds : copy.startSeconds + copy.durationSeconds;
        copy.startSeconds = std::max(0.0, copy.startSeconds);
        for (auto& note : copy.notes) note.id = makeId("note");

        // Connection flags at the outer edges describe neighbouring notes in
        // the original timeline.  Preserve joins inside the copied clip, but
        // do not accidentally glide into unrelated material at its new place.
        if (!copy.notes.empty())
        {
            const auto first = std::min_element(copy.notes.begin(), copy.notes.end(),
                [](const auto& left, const auto& right)
                {
                    return left.startSeconds < right.startSeconds;
                });
            const auto last = std::max_element(copy.notes.begin(), copy.notes.end(),
                [](const auto& left, const auto& right)
                {
                    return left.startSeconds + left.durationSeconds
                        < right.startSeconds + right.durationSeconds;
                });
            first->connectedToPrevious = false;
            last->connectedToNext = false;
        }
        auto relative=copy;relative.startSeconds=0;copy.startSeconds=nativePasteStart(project.tracks[destinationTrackIndex],{relative},copy.startSeconds);
        insertedId = copy.id;
        project.tracks[destinationTrackIndex].clips.push_back(std::move(copy));
    }
    sendChangeMessage();
    return insertedId;
}

void ProjectModel::resizeClip(const juce::String& clipId, double startSeconds,
                              double durationSeconds)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    const auto oldDuration = std::max(0.01, clip.durationSeconds);
                    auto nextDuration = std::max(0.01, durationSeconds);
                    auto nextStart = std::max(0.0, startSeconds);
                    constrainNativeClipResize(track,clipId,nextStart,nextDuration);
                    if (std::abs(clip.startSeconds - nextStart) <= 1.0e-9
                        && std::abs(oldDuration - nextDuration) <= 1.0e-9) return;
                    pushUndoLocked();
                    const auto ratio = nextDuration / oldDuration;
                    const auto native = trackShowsAllNativeRegions(track);
                    if(native && std::abs(ratio-1.0)>1.e-9)
                    {
                        // Freeze the source clock before changing consonant
                        // times/velocity. Its UI velocity range cannot express
                        // arbitrary long stretches without moving source anchors.
                        if(clip.parts.empty())clip.sourceTimeMap=nativeClipClock(clip);
                        else for(const auto& source:expandedClipParts(clip))
                            for(auto& part:clip.parts)if(source.id==clip.id+":"+part.id)
                            {
                                part.sourceTimeMap=nativeClipClock(source);
                                for(auto& point:part.sourceTimeMap)
                                    point.targetSeconds+=source.startSeconds-clip.startSeconds-part.startSeconds;
                            }
                        rememberNativeTrimSources(clip);
                    }
                    const auto scaleGains=[&](ClipData& c)
                    {
                        if(!native)return;
                        for(auto& p:c.gainEnvelope)p.timeSeconds*=ratio;
                        for(auto& layer:c.inheritedGainEnvelopes)for(auto& p:layer)p.timeSeconds*=ratio;
                    };
                    scaleGains(clip);
                    clip.audioStartSeconds *= ratio;
                    if (clip.audioDurationSeconds >= 0.0) clip.audioDurationSeconds *= ratio;
                    for(auto& part:clip.parts)
                    {
                        part.startSeconds*=ratio;part.durationSeconds*=ratio;part.audioStartSeconds*=ratio;
                        if(part.audioDurationSeconds>=0)part.audioDurationSeconds*=ratio;
                        for(auto& point:part.sourceTimeMap)point.targetSeconds*=ratio;
                        for(auto& point:part.nativeTrimClock)point.targetSeconds*=ratio;
                        scaleGains(part);
                        part.fadeInSeconds*=ratio;part.fadeOutSeconds*=ratio;
                        part.crossfadeInSeconds*=ratio;part.crossfadeOutSeconds*=ratio;
                    }
                    for (auto& note : clip.notes)
                    {
                        note.startSeconds *= ratio;
                        note.durationSeconds *= ratio;
                        note.consonantSeconds *= ratio;
                        // attackSpeed is the source-time / element-time slope.
                        // Keep the source Attack boundary fixed while its target
                        // position stretches with the rest of the clip.
                        note.attackSpeed = juce::jlimit(0.05f, 20.0f,
                            note.attackSpeed / static_cast<float>(ratio));
                        for (auto& point : note.contour) point.timeSeconds *= ratio;
                        for (auto& point : note.pitchControlPoints) point.timeSeconds *= ratio;
                        for (auto& point : note.diffSingerPitchReference) point.timeSeconds *= ratio;
                        for (auto& point : note.diffSingerPitchOffset) point.timeSeconds *= ratio;
                        if(native)
                        {
                            for(auto& point:note.amplitudeEnvelope)point.timeSeconds*=ratio;
                            note.vibratoReferenceDurationSeconds*=ratio;
                            note.vibratoTimeOffsetSeconds*=ratio;
                        }
                        mapDiffSingerParameterTimes(note,[&](double time){return time*ratio;});
                        for (auto& marker : note.sibilantMarkers) marker *= ratio;
                    }
                    // The selected source range is unchanged by a timeline
                    // stretch.  Move only the target side of the imported warp
                    // so the original Melodyne Attack/vowel source anchors are
                    // retained exactly.
                    for (auto& point : clip.sourceTimeMap)
                        point.targetSeconds *= ratio;
                    for (auto& point : clip.nativeTrimClock) point.targetSeconds *= ratio;
                    clip.fadeInSeconds = std::min(nextDuration, clip.fadeInSeconds * ratio);
                    clip.fadeOutSeconds = std::min(nextDuration, clip.fadeOutSeconds * ratio);
                    clip.crossfadeInSeconds = std::min(nextDuration, clip.crossfadeInSeconds * ratio);
                    clip.crossfadeOutSeconds = std::min(nextDuration, clip.crossfadeOutSeconds * ratio);
                    clip.startSeconds = nextStart;
                    clip.durationSeconds = nextDuration;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::trimClip(const juce::String& clipId, double startSeconds, double durationSeconds)
{
    if (!std::isfinite(startSeconds) || !std::isfinite(durationSeconds)
        || startSeconds < 0.0 || durationSeconds < 0.01) return false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            const auto find = [&]() { return std::find_if(track.clips.begin(), track.clips.end(),
                [&](const auto& c) { return c.id == clipId; }); };
            auto found = find();
            if (found == track.clips.end()) continue;
            const auto oldStart = found->startSeconds;
            const auto oldEnd = oldStart + found->durationSeconds;
            const auto end = startSeconds + durationSeconds;
            if (std::abs(startSeconds - oldStart) < 1.0e-9 && std::abs(end - oldEnd) < 1.0e-9) return false;
            const auto overlaps = std::min(end, oldEnd) - std::max(startSeconds, oldStart) >= 0.01;
            if (overlaps && ((startSeconds > oldStart && !canSplitClipAt(clipId, startSeconds))
                || (end < oldEnd && !canSplitClipAt(clipId, end)))) return false;
            pushUndoLocked();
            const juce::ScopedValueSetter<bool> grouping(suppressNestedUndo, true);
            if (overlaps)
            {
                if (startSeconds > oldStart)
                {
                    const auto right = splitClipAt(clipId, startSeconds);
                    std::erase_if(track.clips, [&](const auto& c) { return c.id == clipId; });
                    for (auto& c : track.clips) if (c.id == right) c.id = clipId;
                }
                if (end < oldEnd)
                {
                    const auto right = splitClipAt(clipId, end);
                    std::erase_if(track.clips, [&](const auto& c) { return c.id == right; });
                }
                found = find();
                const auto shift = found->startSeconds - startSeconds;
                found->audioDurationSeconds = found->audioLength();
                found->audioStartSeconds += shift;
                shiftClipGainEnvelopes(*found,shift);
                for(auto& part:found->parts)part.startSeconds+=shift;
                for (auto& note : found->notes) note.startSeconds += shift;
                for (auto& point : found->sourceTimeMap) point.targetSeconds += shift;
                for (auto& point : found->nativeTrimClock) point.targetSeconds += shift;
            }
            else
            {
                found->notes.clear(); found->sourceTimeMap.clear();
                found->parts.clear();
                found->gainEnvelope.clear();found->inheritedGainEnvelopes.clear();
                found->audioStartSeconds = 0.0; found->audioDurationSeconds = 0.0;
                found->sourceDurationSeconds = 0.0;
                found->fadeInSeconds = found->fadeOutSeconds = 0.0;
                found->crossfadeInSeconds = found->crossfadeOutSeconds = 0.0;
            }
            found->startSeconds = startSeconds;
            found->durationSeconds = durationSeconds;
            anchorClipGainEnvelope(*found);
            found->glideConnectedFromPrevious = found->glideConnectedToNext = false;
            sendChangeMessage();
            return true;
        }
    }
    return false;
}

void ProjectModel::setClipGain(const juce::String& clipId, float gain)
{
    const auto next = juce::jlimit(0.0f, 4.0f, gain);
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    if (std::abs(clip.gain - next) <= 1.0e-6f) return;
                    pushUndoLocked();
                    clip.gain = next;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setClipFades(const juce::String& clipId, double fadeInSeconds,
                                double fadeOutSeconds)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    const auto nextIn = juce::jlimit(0.0, clip.durationSeconds,
                                                     fadeInSeconds);
                    const auto nextOut = juce::jlimit(0.0, clip.durationSeconds,
                                                      fadeOutSeconds);
                    if (std::abs(clip.fadeInSeconds - nextIn) <= 1.0e-9
                        && std::abs(clip.fadeOutSeconds - nextOut) <= 1.0e-9) return;
                    pushUndoLocked();
                    clip.fadeInSeconds = nextIn;
                    clip.fadeOutSeconds = nextOut;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setClipMuted(const juce::String& clipId, bool muted)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    if (clip.muted == muted) return;
                    pushUndoLocked();
                    clip.muted = muted;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setClipNoteHints(const juce::String& clipId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    if (clip.showNoteHints == enabled || (enabled
                        && (track.accompaniment || !track.compose || clip.notes.empty()))) return;
                    pushUndoLocked();
                    clip.showNoteHints = enabled;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setClipNormalDisplay(const juce::String& clipId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    if (clip.showNormalDisplay == enabled || (enabled
                        && (track.accompaniment || !track.compose))) return;
                    pushUndoLocked();
                    clip.showNormalDisplay = enabled;
                    changed = true;
                    break;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::removeClip(const juce::String& clipId)
{
    removeClips({clipId});
}

void ProjectModel::moveClips(const std::vector<juce::String>& clipIds, double deltaSeconds, int trackDelta)
{
    if (!std::isfinite(deltaSeconds)) return;
    {
        const juce::ScopedLock guard(lock);
        double earliest = std::numeric_limits<double>::max();
        auto firstTrack = static_cast<int>(project.tracks.size()), lastTrack = -1;
        const auto selected = [&](const ClipData& clip) {
            return std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end();
        };
        for (int row = 0; row < static_cast<int>(project.tracks.size()); ++row)
            for (const auto& clip : project.tracks[static_cast<std::size_t>(row)].clips)
                if (selected(clip)) {
                    earliest = std::min(earliest, clip.startSeconds);
                    firstTrack = std::min(firstTrack, row); lastTrack = std::max(lastTrack, row);
                }
        if (lastTrack < 0) return;
        deltaSeconds = std::max(-earliest, deltaSeconds);
        trackDelta = juce::jlimit(-firstTrack, static_cast<int>(project.tracks.size())-1-lastTrack, trackDelta);
        deltaSeconds=constrainedNativeClipMoves(project,clipIds,deltaSeconds,trackDelta);
        if (std::abs(deltaSeconds) < 1.0e-9 && trackDelta == 0) return;
        pushUndoLocked();
        if (trackDelta == 0) {
            for (auto& track : project.tracks) for (auto& clip : track.clips)
                if (selected(clip)) clip.startSeconds += deltaSeconds;
        } else {
            // Remove the entire selection before inserting anything. Otherwise
            // a clip transferred downwards could be moved a second time.
            std::vector<std::pair<std::size_t, ClipData>> moved;
            for (int row = 0; row < static_cast<int>(project.tracks.size()); ++row) {
                auto& clips = project.tracks[static_cast<std::size_t>(row)].clips;
                for (auto it = clips.begin(); it != clips.end();) {
                    if (!selected(*it)) { ++it; continue; }
                    it->startSeconds += deltaSeconds;
                    moved.emplace_back(static_cast<std::size_t>(row+trackDelta), std::move(*it));
                    it = clips.erase(it);
                }
            }
            for (auto& [row, clip] : moved) project.tracks[row].clips.push_back(std::move(clip));
            for (auto& track : project.tracks)
                std::stable_sort(track.clips.begin(), track.clips.end(), [](const auto& a, const auto& b) {
                    return a.startSeconds < b.startSeconds;
                });
        }
    }
    sendChangeMessage();
}

void ProjectModel::removeClips(const std::vector<juce::String>& clipIds)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            const auto selected = [&](const auto& clip)
            { return std::find(clipIds.begin(), clipIds.end(), clip.id) != clipIds.end(); };
            if (!std::any_of(track.clips.begin(), track.clips.end(), selected)) continue;
            if (!changed) pushUndoLocked();
            std::erase_if(track.clips, selected);
            changed = true;
        }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::removeTrack(const juce::String& trackId)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        const auto found = std::find_if(project.tracks.begin(), project.tracks.end(),
            [&](const auto& track) { return track.id == trackId; });
        if (found != project.tracks.end())
        {
            pushUndoLocked();
            project.tracks.erase(found);
            changed = true;
        }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::transposeNote(const juce::String& noteId, float semitones)
{
    transposeNotes({ noteId }, semitones);
}

void ProjectModel::transposeNotes(const std::vector<juce::String>& noteIds, float semitones)
{
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    changed = changed || includes(note.id);
        if (!changed) return;
        pushUndoLocked();
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (includes(note.id))
                    {
                        const auto previous = note.midiNote;
                        note.midiNote = juce::jlimit(0.0f, 127.0f, note.midiNote + semitones);
                        const auto applied = note.midiNote - previous;
                        for (auto& point : note.pitchControlPoints)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                        for (auto& point : note.diffSingerPitchReference)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                    }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::trimNativeNoteEdge(const juce::String& id, double delta, bool left)
{
    if (!std::isfinite(delta) || std::abs(delta) < 1.e-9) return false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks) if (trackShowsAllNativeRegions(track))
            for (auto& clip : track.clips)
                if (auto plan = planNativeNoteTrim(clip, id, delta, left, track.allowNativeAudioOverlap))
                {
                    const auto safe=constrainedNativeTrimDelta(track,id,plan->delta,left);
                    plan=planNativeNoteTrim(clip,id,safe,left,track.allowNativeAudioOverlap);
                    if(!plan)return false;
                    if (std::abs(plan->delta) < 1.e-9) return false;
                    pushUndoLocked(); clip = std::move(plan->clip);
                    const auto cut = [&](const auto& link) { return left ? link.rightNoteId == id : link.leftNoteId == id; };
                    for (auto& t : project.tracks) for (auto& c : t.clips) for (auto& n : c.notes)
                        for (const auto& link : project.nativeConnections) if (cut(link))
                        {
                            if (n.id == link.leftNoteId) n.connectedToNext = false;
                            if (n.id == link.rightNoteId) n.connectedToPrevious = false;
                        }
                    std::erase_if(project.nativeConnections, cut);
                    sendChangeMessage(); return true;
                }
    }
    return false;
}

bool ProjectModel::resizeNativeNoteEdge(const juce::String& id, double delta, bool left)
{
    if (!std::isfinite(delta) || std::abs(delta)<1.e-9) return false;
    bool changed=false;
    {
        const juce::ScopedLock guard(lock);
        for(auto& track:project.tracks) if(trackShowsAllNativeRegions(track))
            for(auto& clip:track.clips)
                if(auto plan=planNativeNoteMove(clip,{id},delta,0,
                    left?NativeNoteTimeEdit::leftEdge:NativeNoteTimeEdit::rightEdge))
                {
                    const auto safe=constrainedNativeNoteDelta(track,{id},plan->delta,
                        left?NativeNoteTimeEdit::leftEdge:NativeNoteTimeEdit::rightEdge);
                    plan=planNativeNoteMove(clip,{id},safe,0,left?NativeNoteTimeEdit::leftEdge:NativeNoteTimeEdit::rightEdge);
                    if(!plan||std::abs(plan->delta)<1.e-9)return false;
                    pushUndoLocked();clip=std::move(plan->clip);changed=true;
                    break;
                }
    }
    if(changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::moveNativeNotes(const std::vector<juce::String>& ids,
                                   double seconds, float semitones, juce::String* error)
{
    if (error) error->clear();
    if (ids.empty() || !std::isfinite(seconds) || !std::isfinite(semitones)) return false;
    bool changed = false;
    {
        const juce::ScopedLock guard(lock);
        std::vector<ClipData*> owners;
        std::size_t count = 0;
        for (auto& track : project.tracks) for (auto& clip : track.clips)
        {
            const auto found = std::count_if(clip.notes.begin(), clip.notes.end(), [&](const auto& note)
                { return std::find(ids.begin(), ids.end(), note.id) != ids.end(); });
            if (!found) continue;
            if (!trackShowsAllNativeRegions(track)) return false;
            owners.push_back(&clip); count += found;
        }
        if (count != ids.size()) return false;
        auto delta = seconds; float low = -127, high = 127;
        for (const auto* clip : owners)
        {
            for (const auto& note : clip->notes) if (std::find(ids.begin(),ids.end(),note.id)!=ids.end())
            { low=std::max(low,-note.midiNote); high=std::min(high,127-note.midiNote); }
            const auto plan = planNativeNoteMove(*clip, ids, seconds, 0);
            if (!plan) return false;
            delta = seconds < 0 ? std::max(delta,plan->delta) : std::min(delta,plan->delta);
        }
        for(const auto& track:project.tracks)
        {const auto safe=constrainedNativeNoteDelta(track,ids,delta);delta=delta<0?std::max(delta,safe):std::min(delta,safe);}
        const auto pitch = juce::jlimit(low,high,semitones);
        if (std::abs(delta)<1.e-9 && std::abs(pitch)<1.e-6) return false;
        std::vector<NativeNoteMovePlan> plans;
        for (const auto* clip : owners)
        {
            auto plan = planNativeNoteMove(*clip,ids,delta,pitch);
            if (!plan) return false;
            plans.push_back(std::move(*plan));
        }
        pushUndoLocked();
        for (std::size_t i=0;i<owners.size();++i) *owners[i]=std::move(plans[i].clip);
        changed=true;
    }
    if (changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::moveUtauNotes(const std::vector<juce::String>& noteIds,
                                 double deltaSeconds, float semitones, juce::String* error)
{
    if (error) error->clear();
    if (!std::isfinite(deltaSeconds) || !std::isfinite(semitones)) return false;
    if (noteIds.empty()) return false;
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };

    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        TrackData* ownerTrack = nullptr;
        ClipData* ownerClip = nullptr;
        std::vector<ClipData*> ownerClips;
        std::size_t foundCount = 0;
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
            {
                const auto count = static_cast<std::size_t>(std::count_if(
                    clip.notes.begin(), clip.notes.end(),
                    [&](const auto& note) { return includes(note.id); }));
                if (count == 0) continue;
                if (ownerTrack != nullptr && ownerTrack != &track) return false;
                ownerTrack = &track;
                ownerClip = &clip;
                foundCount += count;
                ownerClips.push_back(&clip);
            }
        if (ownerTrack == nullptr || ownerClip == nullptr
            || ownerTrack->pitchAlgorithm != PitchAlgorithm::utau
            || foundCount != noteIds.size())
            return false;

        if (ownerClips.size()>1)
        {
            auto time=deltaSeconds; float low=-127,high=127;
            for(const auto* clip:ownerClips) for(const auto& note:clip->notes) if(includes(note.id))
            {time=std::max(time,-note.startSeconds);low=std::max(low,-note.midiNote);high=std::min(high,127-note.midiNote);}
            const auto pitch=juce::jlimit(low,high,semitones);
            if(std::abs(time)<1.e-9 && std::abs(pitch)<1.e-6f)return false;
            // A group spanning regions must stay rigid. Do not independently
            // insert/ripple one region, which would silently change the rhythm.
            if(std::abs(time)>1.e-9)
                for(const auto* clip:ownerClips) for(const auto& note:clip->notes) if(includes(note.id))
                    for(const auto& other:clip->notes) if(!includes(other.id)
                        && note.startSeconds+time<other.startSeconds+other.durationSeconds-1.e-9
                        && note.startSeconds+time+note.durationSeconds>other.startSeconds+1.e-9)
                    {
                        if(error)*error=juce::String::fromUTF8("无法整体移动：目标位置与未选音符重叠。选区和音符均已保留。");
                        return false;
                    }
            pushUndoLocked();
            for(auto* clip:ownerClips)
            {
                for(auto& note:clip->notes) if(includes(note.id))
                {
                    note.startSeconds+=time;note.midiNote+=pitch;
                    for(auto& point:note.pitchControlPoints)point.targetMidi=juce::jlimit(0.f,127.f,point.targetMidi+pitch);
                    for(auto& point:note.diffSingerPitchReference)point.targetMidi=juce::jlimit(0.f,127.f,point.targetMidi+pitch);
                    clip->durationSeconds=std::max(clip->durationSeconds,note.startSeconds+note.durationSeconds);
                }
                std::stable_sort(clip->notes.begin(),clip->notes.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
            }
            sendChangeMessage();return true;
        }

        auto phraseStart = std::numeric_limits<double>::max();
        auto phraseEnd = 0.0;
        auto minimumPitchDelta = -127.0f;
        auto maximumPitchDelta = 127.0f;
        for (const auto& note : ownerClip->notes)
            if (includes(note.id))
            {
                phraseStart = std::min(phraseStart, note.startSeconds);
                phraseEnd = std::max(phraseEnd,
                    note.startSeconds + note.durationSeconds);
                minimumPitchDelta = std::max(minimumPitchDelta, -note.midiNote);
                maximumPitchDelta = std::min(maximumPitchDelta, 127.0f - note.midiNote);
            }
        if (phraseStart == std::numeric_limits<double>::max()) return false;

        const auto phraseDuration = std::max(0.01, phraseEnd - phraseStart);
        auto destination = std::max(0.0, phraseStart + deltaSeconds);
        const auto appliedPitch = juce::jlimit(
            minimumPitchDelta, maximumPitchDelta, semitones);

        // Treat the selected notes as one phrase.  UTAU is monophonic, so a
        // time collision is meaningful regardless of the displayed pitch row.
        auto collisionStart = std::numeric_limits<double>::max();
        const auto destinationEnd = destination + phraseDuration;
        for (const auto& note : ownerClip->notes)
            if (!includes(note.id)
                && note.startSeconds < destinationEnd - 1.0e-9
                && note.startSeconds + note.durationSeconds > destination + 1.0e-9)
                collisionStart = std::min(collisionStart, note.startSeconds);

        const auto insertsAtCollision =
            collisionStart != std::numeric_limits<double>::max();
        if (insertsAtCollision)
        {
            // A move is a cut followed by an insert, not a copy followed by
            // an insert.  When the target lies to the right, removing the
            // phrase first moves that target left by exactly phraseDuration.
            destination = collisionStart >= phraseEnd - 1.0e-9
                ? collisionStart - phraseDuration : collisionStart;
        }
        const auto appliedTime = destination - phraseStart;
        changed = std::abs(appliedTime) > 1.0e-9
            || std::abs(appliedPitch) > 1.0e-6f || insertsAtCollision;
        if (!changed) return false;

        pushUndoLocked();
        if (insertsAtCollision)
        {
            // Close the source slot first, then open an equal-sized slot at
            // the destination.  Notes after both locations therefore keep
            // their original absolute positions and no extra gap accumulates.
            for (auto& note : ownerClip->notes)
                if (!includes(note.id)
                    && note.startSeconds >= phraseEnd - 1.0e-9)
                    note.startSeconds -= phraseDuration;
            for (auto& note : ownerClip->notes)
                if (!includes(note.id)
                    && note.startSeconds >= destination - 1.0e-9)
                    note.startSeconds += phraseDuration;
        }

        for (auto& note : ownerClip->notes)
            if (includes(note.id))
            {
                note.startSeconds += appliedTime;
                note.midiNote = juce::jlimit(0.0f, 127.0f,
                                             note.midiNote + appliedPitch);
                for (auto& point : note.pitchControlPoints)
                    point.targetMidi = juce::jlimit(0.0f, 127.0f,
                        point.targetMidi + appliedPitch);
                for (auto& point : note.diffSingerPitchReference)
                    point.targetMidi = juce::jlimit(0.0f, 127.0f,
                        point.targetMidi + appliedPitch);
            }

        std::stable_sort(ownerClip->notes.begin(), ownerClip->notes.end(),
            [](const auto& left, const auto& right)
            {
                return left.startSeconds < right.startSeconds;
            });
        for (const auto& note : ownerClip->notes)
            ownerClip->durationSeconds = std::max(ownerClip->durationSeconds,
                note.startSeconds + note.durationSeconds);
    }
    if (changed) sendChangeMessage();
    return changed;
}

void ProjectModel::setNotesMidi(const std::vector<juce::String>& noteIds, float midiNote)
{
    const auto target = juce::jlimit(0.0f, 127.0f, midiNote);
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    changed = changed || (includes(note.id)
                        && std::abs(note.midiNote - target) > 1.0e-6f);
        if (!changed) return;
        pushUndoLocked();
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (includes(note.id))
                    {
                        const auto applied = target - note.midiNote;
                        note.midiNote = target;
                        for (auto& point : note.pitchControlPoints)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                        for (auto& point : note.diffSingerPitchReference)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                    }
    }
    sendChangeMessage();
}

void ProjectModel::averageNotesMidi(const std::vector<juce::String>& noteIds)
{
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto sum = 0.0;
    auto count = 0;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    if (includes(note.id))
                    {
                        sum += note.midiNote;
                        ++count;
                    }
    }
    if (count > 0) setNotesMidi(noteIds, static_cast<float>(sum / count));
}

void ProjectModel::quantizeNotesMidi(const std::vector<juce::String>& noteIds,
                                     float stepSemitones)
{
    const auto step = juce::jlimit(0.01f, 12.0f, stepSemitones);
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    if (includes(note.id))
                    {
                        const auto target = juce::jlimit(0.0f, 127.0f,
                            std::round(note.midiNote / step) * step);
                        changed = changed || std::abs(note.midiNote - target) > 1.0e-6f;
                    }
        if (!changed) return;
        pushUndoLocked();
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (includes(note.id))
                    {
                        const auto target = juce::jlimit(0.0f, 127.0f,
                            std::round(note.midiNote / step) * step);
                        const auto applied = target - note.midiNote;
                        note.midiNote = target;
                        for (auto& point : note.pitchControlPoints)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                        for (auto& point : note.diffSingerPitchReference)
                            point.targetMidi = juce::jlimit(0.0f, 127.0f,
                                point.targetMidi + applied);
                    }
    }
    sendChangeMessage();
}

void ProjectModel::removeNotesRippling(const std::vector<juce::String>& noteIds)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
            {
                // The stretch the chosen notes take up, from the first start
                // to the last end.  What follows closes up against what came
                // before, whether either of those is a note or a silence.
                auto from = std::numeric_limits<double>::max();
                auto to = -std::numeric_limits<double>::max();
                for (const auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end())
                    {
                        from = std::min(from, note.startSeconds);
                        to = std::max(to, note.startSeconds + note.durationSeconds);
                    }
                if (to <= from) continue;
                const auto span = to - from;
                if (!changed) pushUndoLocked();
                changed = true;
                std::erase_if(clip.notes, [&noteIds](const auto& note)
                {
                    return std::find(noteIds.begin(), noteIds.end(), note.id)
                        != noteIds.end();
                });
                for (auto& note : clip.notes)
                    if (note.startSeconds >= to - 1.0e-9)
                        note.startSeconds = std::max(0.0, note.startSeconds - span);
                auto needed = 0.0;
                for (const auto& note : clip.notes)
                    needed = std::max(needed, note.startSeconds + note.durationSeconds);
                clip.durationSeconds = std::max(0.01,
                    std::max(needed, clip.durationSeconds - span));
            }
        dropEmptyRecordedClipsLocked();
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::insertGapBeforeNote(const juce::String& noteId, double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            // Where in the track the silence opens, in absolute seconds.  Only
            // the track holding the note is touched: a gap is an edit to one
            // part, not to the piece.
            auto at = -1.0;
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    if (note.id == noteId) at = clip.startSeconds + note.startSeconds;
            if (at < 0.0) continue;
            if (!changed) pushUndoLocked();
            changed = true;
            for (auto& clip : track.clips)
            {
                // A clip that begins after the point moves whole; the one the
                // note is in keeps its start and grows, with the notes from
                // there on carried along.
                if (clip.startSeconds >= at - 1.0e-9)
                {
                    clip.startSeconds += seconds;
                    continue;
                }
                const auto local = at - clip.startSeconds;
                auto moved = false;
                for (auto& note : clip.notes)
                    if (note.startSeconds >= local - 1.0e-9)
                    {
                        note.startSeconds += seconds;
                        moved = true;
                    }
                if (!moved) continue;
                auto needed = 0.0;
                for (const auto& note : clip.notes)
                    needed = std::max(needed, note.startSeconds + note.durationSeconds);
                clip.durationSeconds = std::max(needed, clip.durationSeconds + seconds);
            }
        }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::closeGapBeforeNote(const juce::String& noteId, double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            auto at = -1.0;
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    if (note.id == noteId) at = clip.startSeconds + note.startSeconds;
            if (at < 0.0) continue;
            // Never past the front of the piece: the silence being closed is
            // only as long as there is room to close it into.
            const auto span = std::min(seconds, at);
            if (span <= 0.0) continue;
            if (!changed) pushUndoLocked();
            changed = true;
            for (auto& clip : track.clips)
            {
                if (clip.startSeconds >= at - 1.0e-9)
                {
                    clip.startSeconds = std::max(0.0, clip.startSeconds - span);
                    continue;
                }
                const auto local = at - clip.startSeconds;
                auto moved = false;
                for (auto& note : clip.notes)
                    if (note.startSeconds >= local - 1.0e-9)
                    {
                        note.startSeconds = std::max(0.0, note.startSeconds - span);
                        moved = true;
                    }
                if (!moved) continue;
                auto needed = 0.0;
                for (const auto& note : clip.notes)
                    needed = std::max(needed, note.startSeconds + note.durationSeconds);
                clip.durationSeconds = std::max(0.01,
                    std::max(needed, clip.durationSeconds - span));
            }
        }
    }
    if (changed) sendChangeMessage();
}

std::vector<juce::String> ProjectModel::notesOverlapping(
    const juce::String& clipId, double fromSeconds, double toSeconds) const
{
    std::vector<juce::String> found;
    if (toSeconds <= fromSeconds) return found;
    const juce::ScopedLock guard(lock);
    for (const auto& track : project.tracks)
        for (const auto& clip : track.clips)
        {
            if (clip.id != clipId) continue;
            for (const auto& note : clip.notes)
            {
                // Touching end to end is not overlapping: a phrase pasted
                // against the one before it replaces nothing.
                const auto start = clip.startSeconds + note.startSeconds;
                if (start < toSeconds - 1.0e-9
                    && start + note.durationSeconds > fromSeconds + 1.0e-9)
                    found.push_back(note.id);
            }
        }
    return found;
}

std::vector<juce::String> ProjectModel::insertNotes(
    const juce::String& clipId, const std::vector<NoteData>& noteTemplates,
    double absoluteStartSeconds)
{
    std::vector<juce::String> inserted;
    if (noteTemplates.empty()) return inserted;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.id == clipId)
                {
                    if (clip.durationSeconds <= 0.0) return inserted;
                    const auto utauMode = track.pitchAlgorithm == PitchAlgorithm::utau;
                    // A UTAU phrase may extend the clip it lands in, and so
                    // may anything landing in a clip with no recording behind
                    // it -- that is a span of the timeline, not a length of
                    // audio, the same rule drawing a note follows.  A
                    // recording ends where its audio does, so there a note is
                    // still cropped to what can actually sound.
                    const auto mayStretch = utauMode || !clipIsRecording(clip);
                    const auto localOrigin = mayStretch
                        ? std::max(0.0, absoluteStartSeconds - clip.startSeconds)
                        : juce::jlimit(0.0, clip.durationSeconds,
                            absoluteStartSeconds - clip.startSeconds);
                    std::vector<NoteData> candidates;
                    candidates.reserve(noteTemplates.size());
                    for (const auto& noteTemplate : noteTemplates)
                    {
                        auto note = noteTemplate;
                        note.startSeconds = localOrigin
                            + std::max(0.0, noteTemplate.startSeconds);
                        if (mayStretch)
                        {
                            // Copy every musical and UTAU-specific field
                            // verbatim.  Only the new object's id changes.
                            candidates.push_back(std::move(note));
                            continue;
                        }
                        const auto remaining = clip.durationSeconds - note.startSeconds;
                        if (remaining < 0.01) continue;
                        note.durationSeconds = std::min(
                            std::max(0.01, noteTemplate.durationSeconds), remaining);
                        note.consonantSeconds = std::min(note.consonantSeconds,
                                                        note.durationSeconds);
                        for (auto& point : note.contour)
                            point.timeSeconds = juce::jlimit(0.0,
                                note.durationSeconds, point.timeSeconds);
                        for (auto& point : note.pitchControlPoints)
                            point.timeSeconds = juce::jlimit(0.0,
                                note.durationSeconds, point.timeSeconds);
                        for (auto& point : note.diffSingerPitchReference)
                            point.timeSeconds = juce::jlimit(0.0,
                                note.durationSeconds, point.timeSeconds);
                        for (auto& point : note.diffSingerPitchOffset)
                            point.timeSeconds = juce::jlimit(0.0,
                                note.durationSeconds, point.timeSeconds);
                        for (auto& marker : note.sibilantMarkers)
                            marker = juce::jlimit(0.0, note.durationSeconds, marker);
                        candidates.push_back(std::move(note));
                    }
                    if (candidates.empty()) return inserted;
                    pushUndoLocked();
                    if (utauMode)
                    {
                        const auto phraseStart = std::min_element(
                            candidates.begin(), candidates.end(),
                            [](const auto& left, const auto& right)
                            {
                                return left.startSeconds < right.startSeconds;
                            })->startSeconds;
                        auto phraseEnd = phraseStart;
                        for (const auto& note : candidates)
                            phraseEnd = std::max(phraseEnd,
                                note.startSeconds + note.durationSeconds);
                        const auto phraseDuration = std::max(0.01,
                                                            phraseEnd - phraseStart);
                        auto collisionStart = std::numeric_limits<double>::max();
                        for (const auto& existing : clip.notes)
                            if (existing.startSeconds < phraseEnd - 1.0e-9
                                && existing.startSeconds + existing.durationSeconds
                                    > phraseStart + 1.0e-9)
                                collisionStart = std::min(collisionStart,
                                                          existing.startSeconds);
                        if (collisionStart != std::numeric_limits<double>::max())
                        {
                            for (auto& existing : clip.notes)
                                if (existing.startSeconds >= collisionStart - 1.0e-9)
                                    existing.startSeconds += phraseDuration;
                            const auto align = collisionStart - phraseStart;
                            for (auto& note : candidates)
                                note.startSeconds += align;
                        }
                    }
                    else
                    {
                        candidates.front().connectedToPrevious = false;
                        candidates.back().connectedToNext = false;
                    }
                    inserted.reserve(candidates.size());
                    for (auto& note : candidates)
                    {
                        note.id = makeId("note");
                        inserted.push_back(note.id);
                        clip.notes.push_back(std::move(note));
                    }
                    std::stable_sort(clip.notes.begin(), clip.notes.end(),
                        [](const auto& left, const auto& right)
                        {
                            return left.startSeconds < right.startSeconds;
                        });
                    if (utauMode)
                        for (const auto& note : clip.notes)
                            clip.durationSeconds = std::max(clip.durationSeconds,
                                note.startSeconds + note.durationSeconds);
                    break;
                }
    }
    if (!inserted.empty()) sendChangeMessage();
    return inserted;
}

std::vector<juce::String> ProjectModel::insertNativeAudioClips(
    const juce::String& trackId, const std::vector<ClipData>& templates,
    const std::vector<NativeConnection>& connections, double atSeconds)
{
    std::vector<juce::String> inserted;
    if(templates.empty()||!std::isfinite(atSeconds)||atSeconds<0)return inserted;
    {
        const juce::ScopedLock guard(lock);
        const auto target=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==trackId;});
        if(target==project.tracks.end()||!trackShowsAllNativeRegions(*target))return inserted;
        if(std::any_of(templates.begin(),templates.end(),[](const auto& c){return c.notes.empty()
            ||!c.sourceFile.existsAsFile()||!std::isfinite(c.durationSeconds)||c.durationSeconds<=0;}))return inserted;
        atSeconds=nativePasteStart(*target,templates,atSeconds);
        pushUndoLocked();
        std::map<juce::String,juce::String> noteIds;
        for(auto copy:templates)
        {
            copy.id=makeId("clip");copy.startSeconds+=atSeconds;
            for(auto& note:copy.notes)
            {
                const auto old=note.id;note.id=makeId("note");if(copy.parts.empty())note.clipPartId.clear();
                noteIds[old]=note.id;inserted.push_back(note.id);
            }
            target->clips.push_back(std::move(copy));
        }
        for(auto connection:connections)
        {
            const auto left=noteIds.find(connection.leftNoteId),right=noteIds.find(connection.rightNoteId);
            if(left==noteIds.end()||right==noteIds.end())continue;
            connection.id=makeId("connection");connection.leftNoteId=left->second;connection.rightNoteId=right->second;
            connection.boundarySeconds+=atSeconds;project.nativeConnections.push_back(std::move(connection));
        }
    }
    if(!inserted.empty())sendChangeMessage();
    return inserted;
}

std::vector<juce::String> ProjectModel::duplicateNotes(
    const std::vector<juce::String>& noteIds, const juce::String& targetClipId,
    double absoluteStartSeconds)
{
    const auto data = snapshot();
    const auto native = copyNativeAudioNotes(data, noteIds);
    if (!native.clips.empty())
    {
        juce::String targetTrack;
        for (const auto& track : data.tracks) for (const auto& clip : track.clips)
            if (clip.id == targetClipId || (targetClipId.isEmpty()
                && std::any_of(clip.notes.begin(),clip.notes.end(),[&](const auto& n){return n.id==noteIds.front();})))
                targetTrack=track.id;
        return insertNativeAudioClips(targetTrack,native.clips,native.connections,absoluteStartSeconds);
    }
    std::vector<std::pair<double, NoteData>> found;
    juce::String destination = targetClipId;
    for (const auto& track : data.tracks)
        for (const auto& clip : track.clips)
            for (const auto& note : clip.notes)
                if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end())
                {
                    if (destination.isEmpty()) destination = clip.id;
                    found.emplace_back(clip.startSeconds + note.startSeconds, note);
                }
    if (found.empty() || destination.isEmpty()) return {};
    std::stable_sort(found.begin(), found.end(), [](const auto& left, const auto& right)
    {
        return left.first < right.first;
    });
    const auto origin = found.front().first;
    std::vector<NoteData> templates;
    templates.reserve(found.size());
    for (auto& [absolute, note] : found)
    {
        note.startSeconds = absolute - origin;
        templates.push_back(std::move(note));
    }
    return insertNotes(destination, templates, absoluteStartSeconds);
}

void ProjectModel::resizeNote(const juce::String& noteId, double newStart, double newDuration)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            for (auto& clip : track.clips)
            {
                for (auto& note : clip.notes)
                {
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        const auto oldStart = note.startSeconds;
                        const auto oldDuration = std::max(0.01, note.durationSeconds);
                        const auto oldEnd = oldStart + oldDuration;
                        auto otherEnd = 0.0;
                        for (const auto& candidate : clip.notes)
                            if (candidate.id != noteId)
                                otherEnd = std::max(otherEnd,
                                    candidate.startSeconds + candidate.durationSeconds);
                        const auto wasTail = oldEnd >= otherEnd - 1.0e-6
                            && oldEnd >= clip.durationSeconds - 0.002;
                        const auto targetDuration = std::max(0.01, newDuration);
                        const auto oldAttack = juce::jlimit(0.0, oldDuration,
                            note.consonantSeconds);
                        const auto newAttack = std::min(oldAttack, targetDuration);
                        const auto remapTime = [&](double time)
                        {
                            // A negative UTAU head anchor belongs to the
                            // preutterance before the nominal note.  Resizing
                            // the note body must not collapse it back to zero.
                            if (time < 0.0) return time;
                            const auto clamped = juce::jlimit(0.0, oldDuration, time);
                            if (clamped <= oldAttack || oldDuration <= oldAttack + 1.0e-9)
                                return oldAttack > 1.0e-9
                                    ? clamped * newAttack / oldAttack : 0.0;
                            return newAttack + (clamped - oldAttack)
                                * (targetDuration - newAttack) / (oldDuration - oldAttack);
                        };
                        for (auto& point : note.contour)
                            point.timeSeconds = remapTime(point.timeSeconds);
                        for (auto& point : note.pitchControlPoints)
                            point.timeSeconds = remapTime(point.timeSeconds);
                        for (auto& point : note.diffSingerPitchReference)
                            point.timeSeconds = remapTime(point.timeSeconds);
                        for (auto& point : note.diffSingerPitchOffset)
                            point.timeSeconds = remapTime(point.timeSeconds);
                        mapDiffSingerParameterTimes(note,remapTime);
                        for (auto& marker : note.sibilantMarkers)
                            marker = remapTime(marker);
                        note.startSeconds = std::max(0.0, newStart);
                        note.durationSeconds = targetDuration;
                        note.consonantSeconds = newAttack;
                        if (wasTail)
                            clip.durationSeconds = std::max(0.01,
                                std::max(otherEnd, note.startSeconds + note.durationSeconds));
                        changed = true;
                        break;
                    }
                }
                if (changed) break;
            }
            if (changed) break;
        }
    }
    if (changed) sendChangeMessage();
}

juce::String ProjectModel::splitNote(const juce::String& noteId, double localSeconds)
{
    juce::String createdId;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            for (auto& clip : track.clips)
            {
                for (std::size_t noteIndex = 0; noteIndex < clip.notes.size(); ++noteIndex)
                {
                    if (clip.notes[noteIndex].id != noteId) continue;
                    const auto original = clip.notes[noteIndex];
                    const auto split = juce::jlimit(0.01,
                        std::max(0.01, original.durationSeconds - 0.01), localSeconds);
                    if (split <= 0.0099 || split >= original.durationSeconds - 0.0099)
                        return {};

                    const auto evaluate = [&](double time)
                    {
                        PitchPoint result;
                        result.timeSeconds = time;
                        if (original.contour.empty()) return result;
                        const auto right = std::lower_bound(original.contour.begin(),
                            original.contour.end(), time,
                            [](const PitchPoint& point, double value)
                            {
                                return point.timeSeconds < value;
                            });
                        const auto rightIndex = static_cast<std::size_t>(right == original.contour.end()
                            ? original.contour.size() - 1 : right - original.contour.begin());
                        const auto leftIndex = rightIndex > 0
                            && original.contour[rightIndex].timeSeconds > time
                                ? rightIndex - 1 : rightIndex;
                        const auto& left = original.contour[leftIndex];
                        const auto& next = original.contour[rightIndex];
                        const auto amount = next.timeSeconds > left.timeSeconds
                            ? static_cast<float>(juce::jlimit(0.0, 1.0,
                                (time - left.timeSeconds)
                                    / (next.timeSeconds - left.timeSeconds))) : 0.0f;
                        result.relativeCents = left.relativeCents
                            + (next.relativeCents - left.relativeCents) * amount;
                        result.withoutVibratoCents = left.withoutVibratoCents
                            + (next.withoutVibratoCents - left.withoutVibratoCents) * amount;
                        result.voiced = left.voiced && next.voiced;
                        result.manualTargetCents = left.manualTargetCents
                            + (next.manualTargetCents - left.manualTargetCents) * amount;
                        result.hasManualTarget = left.hasManualTarget && next.hasManualTarget;
                        return result;
                    };

                    auto left = original;
                    auto right = original;
                    left.durationSeconds = split;
                    left.consonantSeconds = std::min(original.consonantSeconds, split);
                    left.connectedToNext = false;
                    right.id = makeId("note");
                    createdId = right.id;
                    right.startSeconds = original.startSeconds + split;
                    right.durationSeconds = original.durationSeconds - split;
                    right.consonantSeconds = original.consonantSeconds > split
                        ? original.consonantSeconds - split : 0.0;
                    right.connectedToPrevious = false;

                    if (trackShowsAllNativeRegions(track))
                    {
                        const auto sources = expandedClipParts(clip, true);
                        const auto source = std::find_if(sources.begin(), sources.end(), [&](const auto& part)
                            { return std::any_of(part.notes.begin(), part.notes.end(),
                                [&](const auto& value) { return value.id == noteId; }); });
                        if (source != sources.end())
                        {
                            bindNativeNoteSource(left, *source);
                            bindNativeNoteSource(right, *source);
                        }
                    }

                    left.contour.clear();
                    right.contour.clear();
                    for (const auto& point : original.contour)
                    {
                        if (point.timeSeconds < split - 1.0e-8)
                            left.contour.push_back(point);
                        if (point.timeSeconds > split + 1.0e-8)
                        {
                            auto shifted = point;
                            shifted.timeSeconds -= split;
                            right.contour.push_back(shifted);
                        }
                    }
                    auto boundary = evaluate(split);
                    boundary.timeSeconds = split;
                    left.contour.push_back(boundary);
                    boundary.timeSeconds = 0.0;
                    right.contour.insert(right.contour.begin(), boundary);

                    left.pitchControlPoints.clear();
                    right.pitchControlPoints.clear();
                    if (!original.pitchControlPoints.empty())
                    {
                        const auto controlPitchAt = [&](double time)
                        {
                            const auto found = std::upper_bound(
                                original.pitchControlPoints.begin(),
                                original.pitchControlPoints.end(), time,
                                [](double value, const PitchCurveEditPoint& point)
                                {
                                    return value < point.timeSeconds;
                                });
                            if (found == original.pitchControlPoints.begin())
                                return found->targetMidi;
                            if (found == original.pitchControlPoints.end())
                                return original.pitchControlPoints.back().targetMidi;
                            const auto& before = *(found - 1);
                            const auto amount = found->timeSeconds > before.timeSeconds
                                ? static_cast<float>((time - before.timeSeconds)
                                    / (found->timeSeconds - before.timeSeconds)) : 0.0f;
                            return before.targetMidi
                                + (found->targetMidi - before.targetMidi) * amount;
                        };
                        for (const auto& point : original.pitchControlPoints)
                        {
                            if (point.timeSeconds < split - 1.0e-8)
                                left.pitchControlPoints.push_back(point);
                            if (point.timeSeconds > split + 1.0e-8)
                            {
                                auto shifted = point;
                                shifted.timeSeconds -= split;
                                right.pitchControlPoints.push_back(shifted);
                            }
                        }
                        const auto boundaryPitch = controlPitchAt(split);
                        left.pitchControlPoints.push_back({ split, boundaryPitch });
                        right.pitchControlPoints.insert(right.pitchControlPoints.begin(),
                            { 0.0, boundaryPitch });
                    }

                    std::erase_if(left.utauFlagCurves,[](const auto& c){return isDiffSingerParameter(c.flag);});
                    std::erase_if(right.utauFlagCurves,[](const auto& c){return isDiffSingerParameter(c.flag);});
                    for(const auto& c:original.utauFlagCurves) if(isDiffSingerParameter(c.flag) && !c.flag.startsWith("DS:PTS:") && !c.points.empty()) {
                        FlagCurve a{c.flag,{}},b{c.flag,{}};
                        for(auto p:c.points) {
                            if(p.timeSeconds<split) a.points.push_back(p);
                            if(p.timeSeconds>split) {p.timeSeconds-=split;b.points.push_back(p);}
                        }
                        const auto value=flagCurveValueAt(c.points,split);
                        a.points.push_back({split,value});b.points.insert(b.points.begin(),{0,value});
                        left.utauFlagCurves.push_back(std::move(a));right.utauFlagCurves.push_back(std::move(b));
                    }
                    left.diffSingerPitchOffset.clear();right.diffSingerPitchOffset.clear();
                    if (!original.diffSingerPitchOffset.empty())
                    {
                        for (const auto& p : original.diffSingerPitchOffset)
                        {
                            if (p.timeSeconds<split) left.diffSingerPitchOffset.push_back(p);
                            if (p.timeSeconds>split) { auto q=p;q.timeSeconds-=split;right.diffSingerPitchOffset.push_back(q); }
                        }
                        const auto value=diffSingerPitchOffsetAt(original,split);
                        left.diffSingerPitchOffset.push_back({split,value,PitchCurveShape::linear});
                        right.diffSingerPitchOffset.insert(right.diffSingerPitchOffset.begin(),{0,value,PitchCurveShape::linear});
                    }
                    left.diffSingerPitchReference.clear();
                    right.diffSingerPitchReference.clear();
                    if (!original.diffSingerPitchReference.empty())
                    {
                        for (const auto& point : original.diffSingerPitchReference)
                        {
                            if (point.timeSeconds < split) left.diffSingerPitchReference.push_back(point);
                            if (point.timeSeconds > split)
                            {
                                auto shifted = point; shifted.timeSeconds -= split;
                                right.diffSingerPitchReference.push_back(shifted);
                            }
                        }
                        const auto midi = evaluatePitchCurve(original.diffSingerPitchReference, split);
                        left.diffSingerPitchReference.push_back({split, midi, PitchCurveShape::linear});
                        right.diffSingerPitchReference.insert(right.diffSingerPitchReference.begin(),
                            {0.0, midi, PitchCurveShape::linear});
                    }

                    left.nativeEnvelope=backend::slicedNativeEnvelope(original.nativeEnvelope,0,split/original.durationSeconds);
                    right.nativeEnvelope=backend::slicedNativeEnvelope(original.nativeEnvelope,split/original.durationSeconds,1);
                    left.amplitudeEnvelope.clear();
                    right.amplitudeEnvelope.clear();
                    if (!original.amplitudeEnvelope.empty())
                    {
                        const auto amplitudeAt = [&](double time)
                        {
                            const auto found = std::upper_bound(
                                original.amplitudeEnvelope.begin(),
                                original.amplitudeEnvelope.end(), time,
                                [](double value, const AmplitudeEnvelopePoint& point)
                                {
                                    return value < point.timeSeconds;
                                });
                            if (found == original.amplitudeEnvelope.begin())
                                return found->gainDb;
                            if (found == original.amplitudeEnvelope.end())
                                return original.amplitudeEnvelope.back().gainDb;
                            const auto& before = *(found - 1);
                            const auto span = found->timeSeconds - before.timeSeconds;
                            const auto amount = span > 1.0e-9
                                ? static_cast<float>(juce::jlimit(0.0, 1.0,
                                    (time - before.timeSeconds) / span)) : 0.0f;
                            return backend::envelopeDbBetween(before.gainDb,
                                found->gainDb, amount, before.linearToNext);
                        };
                        // Whether the stretch the cut lands in runs straight in
                        // amplitude: the half after the cut carries on that way.
                        const auto cutStretchIsLinear = [&]
                        {
                            const auto found = std::upper_bound(
                                original.amplitudeEnvelope.begin(),
                                original.amplitudeEnvelope.end(), split,
                                [](double value, const AmplitudeEnvelopePoint& point)
                                {
                                    return value < point.timeSeconds;
                                });
                            return found != original.amplitudeEnvelope.begin()
                                && found != original.amplitudeEnvelope.end()
                                && (found - 1)->linearToNext;
                        }();
                        for (const auto& point : original.amplitudeEnvelope)
                        {
                            if (point.timeSeconds < split - 1.0e-8)
                                left.amplitudeEnvelope.push_back(point);
                            if (point.timeSeconds > split + 1.0e-8)
                            {
                                auto shifted = point;
                                shifted.timeSeconds -= split;
                                right.amplitudeEnvelope.push_back(shifted);
                            }
                        }
                        const auto boundaryGain = amplitudeAt(split);
                        const auto explicitCut = std::any_of(original.amplitudeEnvelope.begin(), original.amplitudeEnvelope.end(),
                            [split](const auto& p) { return std::abs(p.timeSeconds - split) < 1.e-8; });
                        left.amplitudeEnvelope.push_back({ split, boundaryGain, cutStretchIsLinear, explicitCut });
                        right.amplitudeEnvelope.insert(right.amplitudeEnvelope.begin(),
                            { 0.0, boundaryGain, cutStretchIsLinear, explicitCut });
                    }

                    left.sibilantMarkers.clear();
                    right.sibilantMarkers.clear();
                    for (const auto marker : original.sibilantMarkers)
                        if (marker <= split) left.sibilantMarkers.push_back(marker);
                        else right.sibilantMarkers.push_back(marker - split);

                    pushUndoLocked();
                    if(trackShowsAllNativeRegions(track))rememberNativeTrimSources(clip);
                    clip.notes[noteIndex] = std::move(left);
                    clip.notes.insert(clip.notes.begin()
                        + static_cast<std::ptrdiff_t>(noteIndex + 1), std::move(right));
                    break;
                }
                if (createdId.isNotEmpty()) break;
            }
            if (createdId.isNotEmpty()) break;
        }
    }
    if (createdId.isNotEmpty()) sendChangeMessage();
    return createdId;
}

bool ProjectModel::canJoinNativeNotes(const juce::String& a,const juce::String& b) const
{
    const juce::ScopedLock guard(lock);
    for(const auto& track:project.tracks)for(const auto& clip:track.clips)
        if(planNativeNoteJoin(track,clip,a,b))return true;
    return false;
}

juce::String ProjectModel::joinNativeNotes(const juce::String& a,const juce::String& b)
{
    juce::String kept,removed;
    {
        const juce::ScopedLock guard(lock);
        for(auto& track:project.tracks)for(auto& clip:track.clips)
            if(kept.isEmpty())if(auto joined=planNativeNoteJoin(track,clip,a,b))
            {
                kept=std::any_of(joined->notes.begin(),joined->notes.end(),[&](const auto& n){return n.id==a;})?a:b;
                removed=kept==a?b:a;pushUndoLocked();clip=std::move(*joined);
            }
        if(kept.isEmpty())return {};
        std::erase_if(project.nativeConnections,[&](const auto& c){return(c.leftNoteId==kept&&c.rightNoteId==removed)||(c.leftNoteId==removed&&c.rightNoteId==kept);});
        for(auto& c:project.nativeConnections){if(c.leftNoteId==removed)c.leftNoteId=kept;if(c.rightNoteId==removed)c.rightNoteId=kept;}
    }
    sendChangeMessage();return kept;
}

juce::String ProjectModel::mergeNotes(const std::vector<juce::String>& noteIds)
{
    if (noteIds.size() < 2) return {};
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };

    juce::String mergedId;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            for (auto& clip : track.clips)
            {
                std::vector<std::size_t> selectedIndices;
                for (std::size_t index = 0; index < clip.notes.size(); ++index)
                    if (includes(clip.notes[index].id)) selectedIndices.push_back(index);
                // A merge is deliberately confined to one clip.  This also
                // verifies that every requested note still exists when the
                // asynchronous context-menu command is finally delivered.
                if (selectedIndices.size() != noteIds.size()) continue;

                std::stable_sort(selectedIndices.begin(), selectedIndices.end(),
                    [&](std::size_t left, std::size_t right)
                    {
                        return clip.notes[left].startSeconds
                            < clip.notes[right].startSeconds;
                    });
                const auto firstIndex = selectedIndices.front();
                const auto lastIndex = selectedIndices.back();
                auto merged = clip.notes[firstIndex];
                auto totalDuration = 0.0;
                for (const auto index : selectedIndices)
                    totalDuration += std::max(0.01, clip.notes[index].durationSeconds);

                merged.startSeconds = clip.notes[firstIndex].startSeconds;
                merged.durationSeconds = std::max(0.01, totalDuration);
                merged.label.clear();
                merged.connectedToPrevious = clip.notes[firstIndex].connectedToPrevious;
                merged.connectedToNext = clip.notes[lastIndex].connectedToNext;
                merged.consonantSeconds = std::min(merged.consonantSeconds,
                                                    merged.durationSeconds);
                // Curves from several independent notes use incompatible
                // local time origins.  Start the merged, lyric-less note as a
                // clean flat note instead of retaining misleading fragments.
                merged.contour.clear();
                merged.contour.push_back({ 0.0, 0.0f, 0.0f, true });
                merged.contour.push_back({ merged.durationSeconds, 0.0f, 0.0f, true });
                merged.pitchControlPoints.clear();
                merged.diffSingerPitchReference.clear();
                merged.diffSingerPitchOffset.clear();
                std::erase_if(merged.utauFlagCurves,[](const auto& c){return isDiffSingerParameter(c.flag);});
                merged.diffSingerPitchReferenceFromSavedPitch = false;
                merged.amplitudeEnvelope.clear();
                merged.nativeEnvelope={};
                merged.sibilantMarkers.clear();
                mergedId = merged.id;

                pushUndoLocked();
                clip.notes.erase(std::remove_if(clip.notes.begin(), clip.notes.end(),
                    [&](const auto& note) { return includes(note.id); }), clip.notes.end());
                clip.notes.push_back(std::move(merged));
                std::stable_sort(clip.notes.begin(), clip.notes.end(),
                    [](const auto& left, const auto& right)
                    {
                        return left.startSeconds < right.startSeconds;
                    });
                const auto mergedNote = std::find_if(clip.notes.begin(), clip.notes.end(),
                    [&](const auto& note) { return note.id == mergedId; });
                if (mergedNote != clip.notes.end())
                    clip.durationSeconds = std::max(clip.durationSeconds,
                        mergedNote->startSeconds + mergedNote->durationSeconds);
                break;
            }
            if (mergedId.isNotEmpty()) break;
        }
    }
    if (mergedId.isNotEmpty()) sendChangeMessage();
    return mergedId;
}

void ProjectModel::setNoteModulation(const juce::String& noteId, float modulation)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.modulation = juce::jlimit(0.0f, 2.0f, modulation);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteDrift(const juce::String& noteId, float drift)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.drift = juce::jlimit(0.0f, 2.0f, drift);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteTension(const juce::String& noteId, float tension)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.tension = juce::jlimit(-1.0f, 1.0f, tension);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteBreath(const juce::String& noteId, float breath)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.breath = juce::jlimit(0.0f, 1.0f, breath);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteFormant(const juce::String& noteId, float semitones)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.formantSemitones = juce::jlimit(-12.0f, 12.0f, semitones);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteGain(const juce::String& noteId, float gain)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.gain = juce::jlimit(0.0f, 4.0f, gain);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteAttack(const juce::String& noteId, double consonantSeconds,
                                 float attackSpeed)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        pushUndoLocked();
                        note.consonantSeconds = juce::jlimit(0.0, note.durationSeconds,
                                                            consonantSeconds);
                        note.attackSpeed = juce::jlimit(0.05f, 20.0f, attackSpeed);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::setNotesTailFade(const std::vector<juce::String>& ids,int mode)
{
    if(mode<0||mode>2)return false;bool changed=false;
    {const juce::ScopedLock guard(lock);for(auto& track:project.tracks)
        if(track.pitchAlgorithm==PitchAlgorithm::utau&&!trackIsDiffSinger(track))
        for(auto& clip:track.clips)for(auto& note:clip.notes)
            if(std::find(ids.begin(),ids.end(),note.id)!=ids.end()&&note.utauTailFadeMode!=mode)
            {if(!changed)pushUndoLocked();note.utauTailFadeMode=mode;changed=true;}}
    if(changed)sendChangeMessage();return changed;
}

bool ProjectModel::setNotesTailFade(const std::vector<juce::String>& ids,int mode,const backend::TailFadeSettings& settings)
{
    if(mode<0||mode>2||!settings.valid())return false;bool changed=false;
    {const juce::ScopedLock guard(lock);for(auto& track:project.tracks)
        if(track.pitchAlgorithm==PitchAlgorithm::utau&&!trackIsDiffSinger(track))
        for(auto& clip:track.clips)for(auto& note:clip.notes)
            if(std::find(ids.begin(),ids.end(),note.id)!=ids.end()
                &&(note.utauTailFadeMode!=mode||note.utauTailFade!=settings))
            {if(!changed)pushUndoLocked();note.utauTailFadeMode=mode;note.utauTailFade=settings;changed=true;}}
    if(changed)sendChangeMessage();return changed;
}

bool ProjectModel::setNotesNativeEnvelope(const std::vector<juce::String>& ids,const backend::NativeEnvelopeSettings& settings)
{
    if(!settings.valid())return false;bool changed=false;
    {const juce::ScopedLock guard(lock);for(auto& track:project.tracks)if(trackShowsAllNativeRegions(track))
        for(auto& clip:track.clips)for(auto& note:clip.notes)
            if(std::find(ids.begin(),ids.end(),note.id)!=ids.end()&&note.nativeEnvelope!=settings)
            {if(!changed)pushUndoLocked();note.nativeEnvelope=settings;changed=true;}}
    if(changed)sendChangeMessage();return changed;
}

void ProjectModel::setNotesAmplitudeEnvelopeBase(
    const std::vector<juce::String>& noteIds, float basePercent)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        const auto next = juce::jlimit(0.0f, 200.0f, basePercent);
        auto targets = noteIds;
        for (const auto& track : project.tracks)
        {
            const auto groups = nativeSharedEnvelopes(track);
            for (const auto& id : noteIds)
                if (const auto p = groups.find(id); p != groups.end())
                    for (const auto& [otherId, other] : groups)
                        if (other.points == p->second.points && std::find(targets.begin(), targets.end(), otherId) == targets.end())
                            targets.push_back(otherId);
        }
        auto pushed = false;
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (std::find(targets.begin(), targets.end(), note.id) == targets.end())
                        continue;
                    if (std::abs(note.amplitudeEnvelopeBasePercent - next) <= 1.0e-6f) continue;
                    if (!pushed) { pushUndoLocked(); pushed = true; }
                    note.amplitudeEnvelopeBasePercent = next;
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteAttackSpeed(const juce::String& noteId, float attackSpeed)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId)
                    {
                        const auto next = juce::jlimit(0.05f, 20.0f, attackSpeed);
                        if (std::abs(note.attackSpeed - next) <= 1.0e-6f) return;
                        pushUndoLocked();
                        note.attackSpeed = next;
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

namespace
{
// Changing a lyric is an HJPX edit. Material annotations are written only by
// the explicit material editor, never as a hidden side effect of note editing.
// This keeps UTAU OTO libraries read-only during ordinary project editing.
void relabelNote(NoteData& note, const juce::String& trimmed)
{
    const auto native = !note.nativeSegments.empty();
    const auto label = trimmed.isEmpty() ? juce::String("-") : trimmed;
    const auto oldLabel = note.label;
    note.label = label;
    note.diffSingerTiming.clear();
    note.diffSingerPronunciation.clear();
    // A local OTO/STP belongs to the previous alias even when the note also
    // carries native HJM segments. Keep the annotation but release the override.
    note.utauOto = {};
    note.utauStpSeconds = 0.0;
    if (native)
    {
        note.nativeRole = label == "_" ? NativeSegmentRole::transition
            : label == "-" ? NativeSegmentRole::unknown : NativeSegmentRole::vowel;
        for (auto& segment : note.nativeSegments)
            if (segment.role != NativeSegmentRole::transition
                && (segment.alias == oldLabel || segment.alias == "-"))
                segment.alias = label;
        return;
    }
    note.utauPreutteranceOverrideEnabled = false;
    note.utauPreutteranceSeconds = 0.0;
    note.utauOverlapOverrideEnabled = false;
    note.utauOverlapSeconds = 0.0;
    // An STP is tied to one recording. Changing the alias returns to the
    // selected voicebank entry instead of carrying the old recording offset.
    note.utauStpSeconds = 0.0;
    // A note's own oto is where things are in one recording too, and goes with
    // the rest.
    note.utauOto = {};
    note.utauJieSplitSet = false;
    note.utauJieSplit1 = 0.0;
    note.utauJieSplit2 = 0.0;
    note.utauJieSplit3 = 0.0;
}

// A note a 拼字 note leads into is sung from its own beat, whatever it is
// called.  Retyping its lyric is the first thing anyone does after putting one
// in front of it -- it becomes the vowel -- and relabelling releases the timing
// a note was pinned to, since another recording has its sound somewhere else.
// That released this pin as well, and the consonant was cut off before the
// beat again: heard to 0.96 instead of 1.04 on the check's entry.
//
// The preutterance goes back to nothing.  The overlap stays released, because
// that one does belong to the recording.
void keepVowelOnItsBeat(const ClipData& clip, NoteData& note)
{
    const auto ledIntoByPrefix = std::any_of(clip.notes.begin(), clip.notes.end(),
        [&note](const NoteData& other)
        {
            return other.id != note.id && other.durationSeconds <= 1.0e-12
                && std::abs(other.startSeconds - note.startSeconds) < 1.0e-9;
        });
    if (!ledIntoByPrefix) return;
    note.utauPreutteranceOverrideEnabled = true;
    note.utauPreutteranceSeconds = 0.0;
}
}

void ProjectModel::setNoteLabels(
    const std::vector<std::pair<juce::String, juce::String>>& labels)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& [noteId, label] : labels)
        {
            const auto trimmed = label.trim().isEmpty() ? juce::String("-") : label.trim();
            for (auto& track : project.tracks)
                for (auto& clip : track.clips)
                    for (auto& note : clip.notes)
                        if (note.id == noteId && note.label != trimmed)
                        {
                            // Once for the whole batch, so it undoes as one.
                            if (!changed) pushUndoLocked();
                            relabelNote(note, trimmed);
                            keepVowelOnItsBeat(clip, note);
                            changed = true;
                        }
        }
    }
    if (changed) sendChangeMessage();
}

int ProjectModel::convertTrackLyricsToPinyin(const juce::String& trackId)
{
    std::vector<std::pair<juce::String, juce::String>> labels;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
        {
            // Pinyin is what a UTAU voicebank's aliases are spelt in; a note
            // of any other kind of track is sung from its pitch, not its lyric.
            if (track.id != trackId) continue;
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                {
                    const auto converted = lyricInPinyin(note.label);
                    if (converted != note.label) labels.emplace_back(note.id, converted);
                }
        }
    }
    // The same path a typed lyric takes, so each note is re-read against its
    // voicebank as if it had been typed -- and the whole lot undoes at once.
    setNoteLabels(labels);
    return static_cast<int>(labels.size());
}

void ProjectModel::setNoteLabel(const juce::String& noteId, const juce::String& label)
{
    const auto trimmed = label.trim().isEmpty() ? juce::String("-") : label.trim();
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId && note.label != trimmed)
                    {
                        pushUndoLocked();
                        relabelNote(note, trimmed);
                        keepVowelOnItsBeat(clip, note);
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}


void ProjectModel::setNoteUtauFlags(const juce::String& noteId, const juce::String& flags)
{
    const auto trimmed = flags.trim();
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId && note.utauFlags != trimmed)
                    {
                        pushUndoLocked();
                        note.utauFlags = trimmed;
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauFlags(const std::vector<juce::String>& noteIds,
                                     const juce::String& flags)
{
    if (noteIds.empty()) return;
    const auto trimmed = flags.trim();
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauFlags != trimmed)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauFlags = trimmed;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauConsonantVelocity(
    const std::vector<juce::String>& noteIds, int velocity)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauConsonantVelocity != velocity)
                    {
                        if (!changed) pushUndoLocked();
                        if (track.pitchAlgorithm != PitchAlgorithm::utau
                            && note.consonantSeconds > 1.0e-6
                            && note.consonantSeconds < note.durationSeconds - 1.0e-6)
                        {
                            const auto oldVelocity = note.utauConsonantVelocity
                                == inheritedUtauConsonantVelocity ? 100 : note.utauConsonantVelocity;
                            const auto newVelocity = velocity == inheritedUtauConsonantVelocity
                                ? 100 : velocity;
                            const auto oldBoundary = note.consonantSeconds;
                            const auto newBoundary = juce::jlimit(0.001,
                                std::max(0.001, note.durationSeconds - 0.001),
                                oldBoundary * std::exp2(juce::jlimit(-20.0, 20.0,
                                    (static_cast<double>(oldVelocity) - newVelocity) / 100.0)));
                            const auto remapLocal = [&](double time)
                            {
                                if (time <= 0.0 || time >= note.durationSeconds) return time;
                                return time <= oldBoundary ? time * newBoundary / oldBoundary
                                    : newBoundary + (time - oldBoundary)
                                        * (note.durationSeconds - newBoundary)
                                        / (note.durationSeconds - oldBoundary);
                            };
                            if (clip.sourceTimeMap.empty())
                                clip.sourceTimeMap = { { 0.0, 0.0 },
                                    { clip.durationSeconds, clip.sourceDurationSeconds } };
                            const auto insertAnchor = [&](double time)
                            {
                                auto& map = clip.sourceTimeMap;
                                const auto right = std::lower_bound(map.begin(), map.end(), time,
                                    [](const auto& point, double t) { return point.targetSeconds < t; });
                                if (right == map.begin() || right == map.end()
                                    || std::abs(right->targetSeconds - time) < 1.0e-9) return;
                                const auto& left = *std::prev(right);
                                const auto u = (time-left.targetSeconds)/(right->targetSeconds-left.targetSeconds);
                                const auto source = left.sourceSeconds + u*(right->sourceSeconds-left.sourceSeconds);
                                map.insert(right, { time, source });
                            };
                            insertAnchor(note.startSeconds);
                            insertAnchor(note.startSeconds + oldBoundary);
                            insertAnchor(note.startSeconds + note.durationSeconds);
                            for (auto& point : clip.sourceTimeMap)
                                point.targetSeconds = note.startSeconds
                                    + remapLocal(point.targetSeconds - note.startSeconds);
                            for (auto& point : clip.nativeTrimClock)
                                point.targetSeconds = note.startSeconds + remapLocal(point.targetSeconds - note.startSeconds);
                            for (auto& point : note.contour) point.timeSeconds = remapLocal(point.timeSeconds);
                            for (auto& point : note.pitchControlPoints) point.timeSeconds = remapLocal(point.timeSeconds);
                            for (auto& point : note.diffSingerPitchReference) point.timeSeconds = remapLocal(point.timeSeconds);
                            for (auto& point : note.diffSingerPitchOffset) point.timeSeconds = remapLocal(point.timeSeconds);
                            mapDiffSingerParameterTimes(note,remapLocal);
                            for (auto& point : note.amplitudeEnvelope) point.timeSeconds = remapLocal(point.timeSeconds);
                            for (auto& marker : note.sibilantMarkers) marker = remapLocal(marker);
                            note.attackSpeed *= static_cast<float>(oldBoundary / newBoundary);
                            note.consonantSeconds = newBoundary;
                            note.utauPreutteranceSeconds = newBoundary;
                        }
                        note.utauConsonantVelocity = velocity;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

VibratoSpan vibratoSpanOf(const NoteData& note)
{
    const auto duration = std::max(0.0, note.vibratoReferenceDurationSeconds > 0
        ? note.vibratoReferenceDurationSeconds : note.durationSeconds);
    VibratoSpan span;
    span.end = duration * juce::jlimit(0.0, 100.0, note.vibratoEndPercent) / 100.0;
    span.start = std::max(0.0, span.end
        - duration * juce::jlimit(0.0, 100.0, note.vibratoLengthPercent) / 100.0);
    span.start = juce::jlimit(0.0, note.durationSeconds, span.start - note.vibratoTimeOffsetSeconds);
    span.end = juce::jlimit(0.0, note.durationSeconds, span.end - note.vibratoTimeOffsetSeconds);
    return span;
}

double vibratoCentsAt(const NoteData& note, double localSeconds)
{
    if (!note.vibratoEnabled) return 0.0;
    const auto duration = note.vibratoReferenceDurationSeconds > 0
        ? note.vibratoReferenceDurationSeconds : note.durationSeconds;
    if (duration <= 1.0e-9 || note.vibratoDepthCents == 0.0) return 0.0;
    localSeconds += note.vibratoTimeOffsetSeconds;
    // UTAU measures the vibrato span back from the end of the note; here from
    // where the vibrato ends, which is the note's end unless it was moved.
    VibratoSpan window;
    window.end = duration * juce::jlimit(0.0,100.0,note.vibratoEndPercent) / 100.0;
    window.start = std::max(0.0,window.end-duration*juce::jlimit(0.0,100.0,note.vibratoLengthPercent)/100.0);
    const auto start = window.start;
    const auto span = window.end - window.start;
    if (span <= 1.0e-9) return 0.0;
    if (localSeconds <= start) return 0.0;
    // Past its end the pitch is the note's own again.  At 100 the end is the
    // note's, and nothing reads past that.
    if (localSeconds > window.end + 1.0e-9) return 0.0;
    const auto position = juce::jlimit(0.0, 1.0, (localSeconds - start) / span);

    const auto fadeIn = juce::jlimit(0.0, 100.0, note.vibratoFadeInPercent) / 100.0;
    const auto fadeOut = juce::jlimit(0.0, 100.0, note.vibratoFadeOutPercent) / 100.0;
    auto envelope = 1.0;
    if (fadeIn > 1.0e-9) envelope = std::min(envelope, position / fadeIn);
    if (fadeOut > 1.0e-9) envelope = std::min(envelope, (1.0 - position) / fadeOut);
    envelope = juce::jlimit(0.0, 1.0, envelope);

    const auto cycleSeconds = std::max(0.01, note.vibratoCycleMs) / 1000.0;
    const auto phase = (localSeconds - start) / cycleSeconds
        + note.vibratoPhasePercent / 100.0;
    const auto swing = std::sin(phase * 2.0 * juce::MathConstants<double>::pi);
    const auto centre = note.vibratoOffsetPercent / 100.0;
    return note.vibratoDepthCents * envelope * (swing + centre);
}

void ProjectModel::setNotesUtauSplice(const std::vector<juce::String>& noteIds,
                                      bool enabled)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauSplice != enabled)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauSplice = enabled;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauFlagCurveEnabled(
    const std::vector<juce::String>& noteIds, bool enabled)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            // 线性flag belongs to 界 and 谋.  Switching one off is allowed
            // anywhere: a track moved to plain UTAU with curves still on has to
            // be able to put them away.
            if (enabled && !trackTakesFlagCurves(track)) continue;
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauFlagCurveEnabled != enabled)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauFlagCurveEnabled = enabled;
                        // Nothing is written here.  This used to seed a flat
                        // two-point curve on g so the lane had something to
                        // drag; the lane now draws that starting line itself,
                        // for whichever flag is on show and across the stretch
                        // that flag reaches.  Seeding as well left g -- and
                        // only g -- opening with two handles instead of one,
                        // at the note's own start and end rather than the
                        // stretch it sounds for, and made a note count as
                        // carrying a curve before anything had been drawn.
                        changed = true;
                    }
        }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::resetNotesUtauFlagCurves(
    const std::vector<juce::String>& noteIds)
{
    if (noteIds.empty()) return false;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.utauFlagCurveEnabled
                        && !note.utauFlagCurves.empty()
                        && std::find(noteIds.begin(), noteIds.end(), note.id)
                               != noteIds.end())
                    {
                        if (!changed) pushUndoLocked();
                        std::erase_if(note.utauFlagCurves, [](const auto& c) { return !isDiffSingerParameter(c.flag); });
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::resetNotesUtauFlagCurve(
    const std::vector<juce::String>& noteIds, const juce::String& flag)
{
    if (noteIds.empty() || flag.isEmpty() || flag.startsWith("DS:REF:") || flag.startsWith("DS:PTS:")) return false;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if ((!note.utauFlagCurveEnabled && !flag.startsWith("DS:ABS:"))
                        || std::find(noteIds.begin(),noteIds.end(),note.id)==noteIds.end()) continue;
                    // HiFisampler may display a legacy unprefixed curve. Reset
                    // its native view without deleting the other engine's data.
                    if (flag.startsWith("HIFI:")) {
                        if (flagCurvePointsFor(note,flag).empty()) continue;
                    } else if (std::none_of(note.utauFlagCurves.begin(),note.utauFlagCurves.end(),
                        [&](const auto& curve) {return curve.flag==flag;})) continue;
                    const auto keepLegacy = flag.startsWith("HIFI:")
                        && !flagCurvePointsFor(note,flag.substring(5)).empty();
                    if (!changed) pushUndoLocked();
                    std::erase_if(note.utauFlagCurves,[&](const auto& curve) {
                        return curve.flag==flag || (flag.startsWith("DS:ABS:") && curve.flag=="DS:PTS:"+flag.substring(7));
                    });
                    if (keepLegacy) note.utauFlagCurves.push_back({flag,{}});
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::rememberDiffSingerParameters(const std::map<juce::String, std::vector<FlagCurve>>& curves)
{
    const juce::ScopedLock guard(lock);
    bool changed = false;
    for (auto& track : project.tracks) if (trackIsDiffSinger(track))
    for (auto& clip : track.clips) for (auto& note : clip.notes)
    {
        const auto found = curves.find(note.id);
        if (found == curves.end()) continue;
        for (const auto& incoming : found->second)
        {
            if (!incoming.flag.startsWith("DS:AUTO:") || incoming.points.empty()) continue;
            const auto code = incoming.flag.substring(8);
            if (std::none_of(diffSingerParameterKinds().begin(), diffSingerParameterKinds().end(),
                [&](const auto& k){return juce::String(k.flag)=="DS:ABS:"+code;})) continue;
            // Explicit predictions and edited bases own their original reference.
            if (std::any_of(note.utauFlagCurves.begin(), note.utauFlagCurves.end(), [&](const auto& c){
                return c.flag=="DS:ABS:"+code || c.flag=="DS:REF:"+code;
            })) continue;
            const auto& kind = flagCurveKindFor(incoming.flag);
            auto previous = -1.e30; bool valid = true;
            for (const auto& p : incoming.points) {
                valid = valid && std::isfinite(p.timeSeconds) && std::isfinite(p.value)
                    && p.timeSeconds>previous && p.timeSeconds>=-5 && p.timeSeconds<=60
                    && p.value>=kind.minimum && p.value<=kind.maximum;
                previous=p.timeSeconds;
            }
            if (!valid) continue;
            auto old=std::find_if(note.utauFlagCurves.begin(),note.utauFlagCurves.end(),
                [&](const auto& c){return c.flag==incoming.flag;});
            if (old!=note.utauFlagCurves.end() && old->points.size()==incoming.points.size()
                && std::equal(old->points.begin(),old->points.end(),incoming.points.begin(),
                    [](const auto& a,const auto& b){return a.timeSeconds==b.timeSeconds && a.value==b.value;})) continue;
            if (old==note.utauFlagCurves.end()) note.utauFlagCurves.push_back(incoming);
            else *old=incoming;
            changed=true;
        }
    }
    // Derived results make the document dirty, but must not consume an undo step.
    if (changed) { ++revision; sendChangeMessage(); }
    return changed;
}

bool ProjectModel::applyDiffSingerParameters(std::uint64_t expectedRevision,
    const std::map<juce::String,std::vector<FlagCurve>>& curves)
{
    const juce::ScopedLock guard(lock);
    if(revision!=expectedRevision || curves.empty()) return false;
    for(const auto& [id,items]:curves) for(const auto& c:items) {
        if(!c.flag.startsWith("DS:REF:") || c.points.empty()) return false;
        bool known=false;
        for(const auto& k:diffSingerParameterKinds()) known=known || c.flag=="DS:REF:"+juce::String(k.flag).substring(7);
        if(!known) return false;
        const auto& kind=flagCurveKindFor(c.flag);
        double previous=-1e30;
        for(const auto& p:c.points) {
            if(!std::isfinite(p.timeSeconds)||!std::isfinite(p.value)||p.timeSeconds<=previous
                || p.timeSeconds < -5 || p.timeSeconds>60 || p.value<kind.minimum || p.value>kind.maximum) return false;
            previous=p.timeSeconds;
        }
    }
    bool changed=false;
    for(auto& t:project.tracks) if(trackIsDiffSinger(t)) for(auto& c:t.clips) for(auto& n:c.notes) {
        auto found=curves.find(n.id); if(found==curves.end()) continue;
        if(!changed) pushUndoLocked(); changed=true;
        std::erase_if(n.utauFlagCurves,[](const auto& x){return isDiffSingerParameter(x.flag);});
        n.utauFlagCurves.insert(n.utauFlagCurves.end(),found->second.begin(),found->second.end());
    }
    if(changed) sendChangeMessage();
    return changed;
}

bool ProjectModel::setDiffSingerParameterCurve(const juce::String& id,const juce::String& key,
    std::vector<FlagCurvePoint> points)
{
    const juce::ScopedLock guard(lock);
    for(auto& t:project.tracks) if(trackIsDiffSinger(t)) for(auto& c:t.clips) for(auto& n:c.notes) {
        if(n.id!=id) continue;
        const auto& kind=flagCurveKindFor(key);
        for(auto& p:points) {
            if(!std::isfinite(p.timeSeconds)||!std::isfinite(p.value)) return false;
            if(!std::isfinite(p.bezierX1)||!std::isfinite(p.bezierY1)||!std::isfinite(p.bezierX2)||!std::isfinite(p.bezierY2)) return false;
            p.bezierX1=juce::jlimit(0.0f,1.0f,p.bezierX1);p.bezierX2=juce::jlimit(0.0f,1.0f,p.bezierX2);
            p.bezierY1=juce::jlimit(-2.0f,3.0f,p.bezierY1);p.bezierY2=juce::jlimit(-2.0f,3.0f,p.bezierY2);
            p.timeSeconds=juce::jlimit(-5.0,60.0,p.timeSeconds);
            p.value=juce::jlimit(kind.minimum,kind.maximum,p.value);
        }
        std::stable_sort(points.begin(),points.end(),[](auto& a,auto& b){return a.timeSeconds<b.timeSeconds;});
        points.erase(std::unique(points.begin(),points.end(),[](auto& a,auto& b){return std::abs(a.timeSeconds-b.timeSeconds)<1e-5;}),points.end());
        const auto previous=diffSingerParameterHandles(n,key);
        if(points.size()==previous.size() && std::equal(points.begin(),points.end(),previous.begin(),[](auto& a,auto& b){
            return a.timeSeconds==b.timeSeconds && a.value==b.value && a.shape==b.shape
                && a.bezierX1==b.bezierX1 && a.bezierY1==b.bezierY1 && a.bezierX2==b.bezierX2 && a.bezierY2==b.bezierY2;
        })) return false;
        auto full=spliceDiffSingerParameterHandles(n,key,points);
        pushUndoLocked();
        std::erase_if(n.utauFlagCurves,[&](const auto& x){return x.flag==key||x.flag=="DS:PTS:"+key.substring(7);});
        if(!points.empty()) {
            n.utauFlagCurves.push_back({key,std::move(full)});
            n.utauFlagCurves.push_back({"DS:PTS:"+key.substring(7),std::move(points)});
        }
        sendChangeMessage(); return true;
    }
    return false;
}

bool ProjectModel::setNoteUtauFlagCurve(const juce::String& noteId,
                                       const juce::String& flag,
                                       std::vector<FlagCurvePoint> points)
{
    if (noteId.isEmpty()) return false;
    if (flag.startsWith("DS:ABS:")) return setDiffSingerParameterCurve(noteId,flag,std::move(points));
    return setNoteUtauFlagCurves(flag, {{noteId, std::move(points)}}, std::nullopt, false);
}

bool ProjectModel::setNoteUtauFlagCurves(const juce::String& flag,
    const std::map<juce::String, std::vector<FlagCurvePoint>>& edits,
    std::optional<std::uint64_t> expectedRevision, bool enabledOnly)
{
    if (flag.isEmpty() || edits.empty() || flag.startsWith("DS:REF:") || flag.startsWith("DS:PTS:")) return false;
    std::map<juce::String, std::vector<FlagCurvePoint>> prepared;
    for (const auto& [id, input] : edits)
    {
        auto points = input;
        const auto& kind = flagCurveKindFor(flag);
        std::erase_if(points, [](const auto& point) { return !std::isfinite(point.timeSeconds) || !std::isfinite(point.value); });
        std::stable_sort(points.begin(), points.end(),
            [](const auto& left, const auto& right)
            {
                return left.timeSeconds < right.timeSeconds;
            });
        std::vector<FlagCurvePoint> normalized;
        normalized.reserve(points.size());
        for (auto point : points)
        {
            if (!std::isfinite(point.timeSeconds) || !std::isfinite(point.value)) continue;
            // A contour-wide shape means nothing to a single segment; it reads as a
            // straight line, so store it as one rather than leaving a value that
            // says something it cannot do.
            if (point.shape == PitchCurveShape::natural) point.shape = PitchCurveShape::linear;
            point.bezierX1 = juce::jlimit(0.0f, 1.0f, point.bezierX1);
            point.bezierX2 = juce::jlimit(0.0f, 1.0f, point.bezierX2);
            point.bezierY1 = juce::jlimit(-2.0f, 3.0f, point.bezierY1);
            point.bezierY2 = juce::jlimit(-2.0f, 3.0f, point.bezierY2);
            point.timeSeconds = juce::jlimit(-5.0, 60.0, point.timeSeconds);
            // The engine clamps this flag to that range either way; holding the
            // same one here keeps what is drawn and what is heard the same thing.
            point.value = juce::jlimit(kind.minimum, kind.maximum, point.value);
            // Two handles at the same instant would be a step the engine reads as
            // one; the later one wins, as it does there.
            if (!normalized.empty()
                && point.timeSeconds <= normalized.back().timeSeconds + 1.0e-5)
                normalized.back() = point;
            else
                normalized.push_back(point);
        }
        prepared.emplace(id, std::move(normalized));
    }
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        // A project edit arriving during a drag must not be overwritten by its preview.
        if (expectedRevision && revision != *expectedRevision) return false;
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    const auto found = prepared.find(note.id);
                    if (found == prepared.end() || (enabledOnly && !note.utauFlagCurveEnabled && !flag.startsWith("DS:ABS:"))) continue;
                    const auto& normalized = found->second;
                    if (!normalized.empty() && (!trackTakesFlagCurves(track)
                        || trackIsDiffSinger(track) != flag.startsWith("DS:"))) continue;
                    const auto existing = flagCurvePointsFor(note, flag);
                    const auto same = existing.size() == normalized.size()
                        && std::equal(existing.begin(),
                                      existing.end(), normalized.begin(),
                            [](const auto& left, const auto& right)
                            {
                                return std::abs(left.timeSeconds - right.timeSeconds) < 1.0e-9
                                    && std::abs(left.value - right.value) < 1.0e-6f
                                    && left.shape == right.shape
                                    && std::abs(left.bezierX1 - right.bezierX1) < 1.0e-6f
                                    && std::abs(left.bezierY1 - right.bezierY1) < 1.0e-6f
                                    && std::abs(left.bezierX2 - right.bezierX2) < 1.0e-6f
                                    && std::abs(left.bezierY2 - right.bezierY2) < 1.0e-6f;
                            });
                    if (same) continue;
                    if (!changed) pushUndoLocked();
                    std::erase_if(note.utauFlagCurves,
                        [&flag](const auto& curve) { return curve.flag == flag; });
                    if (flag.startsWith("DS:ABS:")) std::erase_if(note.utauFlagCurves,
                        [&](const auto& c) { return c.flag=="DS:PTS:"+flag.substring(7); });
                    // An empty native entry blocks legacy fallback, so clearing
                    // the lane really restores text/region flags after reopening.
                    if (!normalized.empty() || (flag.startsWith("HIFI:")
                        && !flagCurvePointsFor(note,flag.substring(5)).empty()))
                        note.utauFlagCurves.push_back({flag, normalized});
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
    return changed;
}

void ProjectModel::setNotesRegionFlags(const std::vector<juce::String>& noteIds,
                                       bool split, const juce::String& first,
                                       const juce::String& second,
                                       const juce::String& third,
                                       const juce::String& fourth)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) == noteIds.end())
                        continue;
                    if (!changed) pushUndoLocked();
                    note.utauFlagSplit = split;
                    note.utauRegionFlags1 = first.trim();
                    note.utauRegionFlags2 = second.trim();
                    note.utauRegionFlags3 = third.trim();
                    note.utauRegionFlags4 = fourth.trim();
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::bakeNoteVibratoIntoPitch(const juce::String& noteId)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (note.id != noteId || !note.vibratoEnabled) continue;
                    if (note.durationSeconds <= 1.0e-9) continue;

                    // Base pitch under the swing: the edited curve if there is
                    // one, otherwise the note's own contour.
                    const auto baseAt = [&note](double time)
                    {
                        if (!note.pitchControlPoints.empty())
                            return evaluatePitchCurve(note.pitchControlPoints, time);
                        if (note.contour.empty()) return note.midiNote;
                        const auto cents = [&note](const PitchPoint& point)
                        {
                            return renderedPitchCents(note, point);
                        };
                        if (time <= note.contour.front().timeSeconds)
                            return note.midiNote + cents(note.contour.front()) / 100.0f;
                        if (time >= note.contour.back().timeSeconds)
                            return note.midiNote + cents(note.contour.back()) / 100.0f;
                        for (std::size_t i = 1; i < note.contour.size(); ++i)
                        {
                            const auto& left = note.contour[i - 1];
                            const auto& right = note.contour[i];
                            if (time > right.timeSeconds) continue;
                            const auto width = right.timeSeconds - left.timeSeconds;
                            const auto amount = width > 1.0e-9
                                ? static_cast<float>((time - left.timeSeconds) / width) : 0.0f;
                            return note.midiNote
                                + (cents(left) + (cents(right) - cents(left)) * amount) / 100.0f;
                        }
                        return note.midiNote + cents(note.contour.back()) / 100.0f;
                    };

                    // A vibrato is a sine, and from one crest to the next
                    // trough a sine is a single curve.  Sampling it eight times
                    // a cycle wrote a hundred points where a dozen would do,
                    // and every one of them then has to be dragged by hand.
                    //
                    // One point per crest and trough, joined by the cubic that
                    // fits a half sine, carries the same shape: handles at
                    // 0.36434 leave a worst-case error of 0.00019 of the swing,
                    // against 0.01 for the plain smooth join -- a fiftieth of a
                    // cent on a hundred-cent vibrato.
                    const auto cycle = std::max(0.02, note.vibratoCycleMs / 1000.0);
                    const auto swingSpan = vibratoSpanOf(note);
                    const auto swingStart = swingSpan.start;
                    const auto swingEnd = swingSpan.end;
                    const auto phase = note.vibratoPhasePercent / 100.0;

                    std::vector<double> times;
                    times.push_back(0.0);
                    if (swingStart > 1.0e-6) times.push_back(swingStart);
                    // A sine is at an extreme a quarter turn in, and every half
                    // turn after that; it crosses zero halfway between.
                    std::vector<double> extremes;
                    for (auto index = 0; index < 8000; ++index)
                    {
                        const auto time = swingStart
                            + cycle * (0.25 + index * 0.5 - phase);
                        if (time >= swingEnd) break;
                        if (time > swingStart + 1.0e-9)
                        {
                            times.push_back(time);
                            extremes.push_back(time);
                        }
                    }
                    // Between two extremes a half turn is one curve and needs
                    // nothing in between.  The stretch before the first and
                    // after the last is whatever the phase leaves over, so a
                    // crossing goes in it -- one point, two in all -- cutting
                    // it into quarter turns the fitted curves do cover.
                    const auto firstExtreme = extremes.empty()
                        ? swingEnd : extremes.front();
                    const auto lastExtreme = extremes.empty()
                        ? swingStart : extremes.back();
                    std::vector<double> crossings;
                    for (auto index = -4; index < 8000; ++index)
                    {
                        const auto time = swingStart + cycle * (index * 0.5 - phase);
                        if (time >= swingEnd - 1.0e-9) break;
                        if (time < swingStart - 1.0e-9) continue;
                        if (time < firstExtreme - 1.0e-9 || time > lastExtreme + 1.0e-9)
                        {
                            times.push_back(time);
                            crossings.push_back(time);
                        }
                    }
                    // The swing's own end, and -- when it stops before the
                    // note does -- the note's line straight after it and at
                    // the note's end, so what follows is the note's own pitch.
                    times.push_back(swingEnd);
                    if (swingEnd < note.durationSeconds - 1.0e-6)
                    {
                        times.push_back(std::min(note.durationSeconds, swingEnd + 0.001));
                        times.push_back(note.durationSeconds);
                    }
                    std::sort(times.begin(), times.end());
                    times.erase(std::unique(times.begin(), times.end(),
                        [](double left, double right)
                        {
                            return std::abs(left - right) < 1.0e-6;
                        }), times.end());

                    std::vector<PitchCurveEditPoint> points;
                    points.reserve(times.size());
                    for (const auto time : times)
                    {
                        PitchCurveEditPoint point;
                        point.timeSeconds = time;
                        point.targetMidi = baseAt(time)
                            + static_cast<float>(vibratoCentsAt(note, time) / 100.0);
                        // What the sine does between this point and the one
                        // before decides how they are joined.  Handles fitted
                        // to each arc: worst case 0.0002 of the swing, against
                        // 0.01 for a plain smooth join and 0.21 for a straight
                        // line -- a fiftieth of a cent on a hundred cent
                        // vibrato, against ten.
                        const auto kindOf = [&](double when)
                        {
                            const auto close = [when](double value)
                            {
                                return std::abs(value - when) < 1.0e-9;
                            };
                            if (std::any_of(extremes.begin(), extremes.end(), close))
                                return 1;   // crest or trough, where it is flat
                            if (std::any_of(crossings.begin(), crossings.end(), close))
                                return 2;   // zero crossing, where it is steepest
                            return 0;
                        };
                        const auto here = kindOf(time);
                        const auto before = points.empty()
                            ? 0 : kindOf(points.back().timeSeconds);
                        point.shape = PitchCurveShape::smooth;
                        if (before == 1 && here == 1)          // half a turn
                        {
                            point.shape = PitchCurveShape::customBezier;
                            point.bezierX1 = 0.36434f;
                            point.bezierY1 = 0.0f;
                            point.bezierX2 = 0.63566f;
                            point.bezierY2 = 1.0f;
                        }
                        else if (before == 2 && here == 1)     // rising quarter
                        {
                            point.shape = PitchCurveShape::customBezier;
                            point.bezierX1 = 0.33125f;
                            point.bezierY1 = 0.52033f;
                            point.bezierX2 = 0.64000f;
                            point.bezierY2 = 1.0f;
                        }
                        else if (before == 1 && here == 2)     // falling quarter
                        {
                            point.shape = PitchCurveShape::customBezier;
                            point.bezierX1 = 0.36000f;
                            point.bezierY1 = 0.0f;
                            point.bezierX2 = 0.66875f;
                            point.bezierY2 = 0.47967f;
                        }
                        points.push_back(point);
                    }

                    pushUndoLocked();
                    note.pitchControlPoints = std::move(points);
                    note.vibratoEnabled = false;
                    note.vibratoRealLine = false;
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
    return changed;
}

void ProjectModel::setNotesVibratoRealLine(const std::vector<juce::String>& noteIds,
                                           bool enabled)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.vibratoRealLine != enabled)
                    {
                        if (!changed) pushUndoLocked();
                        note.vibratoRealLine = enabled;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesVibrato(const std::vector<juce::String>& noteIds,
                                   const NoteData& parameters, bool enabled,
                                   std::optional<bool> realLine)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (note.nativeUnpitched || std::find(noteIds.begin(), noteIds.end(), note.id) == noteIds.end())
                        continue;
                    if (!changed) pushUndoLocked();
                    note.vibratoEnabled = enabled;
                    if (realLine) note.vibratoRealLine = *realLine;
                    note.vibratoLengthPercent = parameters.vibratoLengthPercent;
                    note.vibratoCycleMs = parameters.vibratoCycleMs;
                    note.vibratoDepthCents = parameters.vibratoDepthCents;
                    note.vibratoFadeInPercent = parameters.vibratoFadeInPercent;
                    note.vibratoFadeOutPercent = parameters.vibratoFadeOutPercent;
                    note.vibratoPhasePercent = parameters.vibratoPhasePercent;
                    note.vibratoOffsetPercent = parameters.vibratoOffsetPercent;
                    note.vibratoEndPercent = juce::jlimit(1.0, 100.0, parameters.vibratoEndPercent);
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauJieSplitCleared(const std::vector<juce::String>& noteIds)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauJieSplitSet)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauJieSplitSet = false;
                        note.utauJieSplit1 = 0.0;
                        note.utauJieSplit2 = 0.0;
                        note.utauJieSplit3 = 0.0;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauJieSplit(const std::vector<juce::String>& noteIds,
                                        double first, double second, double third)
{
    if (noteIds.empty()) return;
    // Keep the three boundaries ordered inside the note; a drag may collapse a
    // region to nothing but must never invert one.
    const auto third_ = juce::jlimit(0.0, 1.0, third);
    const auto second_ = juce::jlimit(0.0, third_, second);
    const auto first_ = juce::jlimit(0.0, second_, first);
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && (!note.utauJieSplitSet
                            || note.utauJieSplit1 != first_
                            || note.utauJieSplit2 != second_
                            || note.utauJieSplit3 != third_))
                    {
                        if (!changed) pushUndoLocked();
                        note.utauJieSplitSet = true;
                        note.utauJieSplit1 = first_;
                        note.utauJieSplit2 = second_;
                        note.utauJieSplit3 = third_;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::clearNotesUtauJieSplit(const std::vector<juce::String>& noteIds)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                        && note.utauJieSplitSet)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauJieSplitSet = false;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteUtauOto(const juce::String& noteId,
                                  const backend::UtauOtoOverride& oto)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId && note.utauOto != oto)
                    {
                        if (!changed) pushUndoLocked();
                        note.utauOto = oto;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::clearNotesUtauOto(const std::vector<juce::String>& noteIds)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.utauOto.enabled
                        && std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end())
                    {
                        if (!changed) pushUndoLocked();
                        note.utauOto = {};
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNotesUtauStp(const std::vector<juce::String>& noteIds,
                                   double seconds)
{
    if (noteIds.empty() || !std::isfinite(seconds)) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (std::find(noteIds.begin(), noteIds.end(), note.id)
                        == noteIds.end())
                        continue;
                    if (std::abs(note.utauStpSeconds - seconds) <= 1.0e-12) continue;
                    if (!changed) pushUndoLocked();
                    note.utauStpSeconds = seconds;
                    changed = true;
                }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setNoteUtauTimingOverrides(
    const juce::String& noteId, bool enabled,
    double preutteranceSeconds, double overlapSeconds)
{
    if (!std::isfinite(preutteranceSeconds) || !std::isfinite(overlapSeconds))
        return;
    const auto normalizedPreutterance = std::max(0.0, preutteranceSeconds);
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId
                        && (note.utauPreutteranceOverrideEnabled != enabled
                            || note.utauOverlapOverrideEnabled != enabled
                            || (enabled && (std::abs(note.utauPreutteranceSeconds
                                                    - normalizedPreutterance) > 1.0e-9
                                || std::abs(note.utauOverlapSeconds
                                            - overlapSeconds) > 1.0e-9))))
                    {
                        pushUndoLocked();
                        note.utauPreutteranceOverrideEnabled = enabled;
                        note.utauPreutteranceSeconds = normalizedPreutterance;
                        note.utauOverlapOverrideEnabled = enabled;
                        note.utauOverlapSeconds = overlapSeconds;
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::setNoteAmplitudeEnvelope(
    const juce::String& noteId, std::vector<AmplitudeEnvelopePoint> points)
{
    std::vector<std::pair<juce::String, std::vector<AmplitudeEnvelopePoint>>> envelopes;
    envelopes.emplace_back(noteId, std::move(points));
    return setNotesAmplitudeEnvelopes(std::move(envelopes));
}

bool ProjectModel::setNotesAmplitudeEnvelopes(
    std::vector<std::pair<juce::String, std::vector<AmplitudeEnvelopePoint>>> envelopes)
{
    std::erase_if(envelopes, [](auto& entry)
    {
        auto& points = entry.second;
        std::stable_sort(points.begin(), points.end(), [](const auto& left, const auto& right)
        {
            return left.timeSeconds < right.timeSeconds;
        });
        std::vector<AmplitudeEnvelopePoint> normalized;
        normalized.reserve(points.size());
        for (auto point : points)
        {
            if (!std::isfinite(point.timeSeconds) || !std::isfinite(point.gainDb)) continue;
            point.timeSeconds = juce::jlimit(-5.0, 60.0, point.timeSeconds);
            point.gainDb = juce::jlimit(-60.0f, 12.0f, point.gainDb);
            if (!normalized.empty()
                && point.timeSeconds <= normalized.back().timeSeconds + 1.0e-5)
                continue;
            normalized.push_back(point);
        }
        points = std::move(normalized);
        return entry.first.isEmpty() || points.size() < 2;
    });
    if (envelopes.empty()) return false;

    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (const auto entry = std::find_if(envelopes.begin(), envelopes.end(),
                            [&](const auto& value) { return value.first == note.id; });
                        entry != envelopes.end())
                    {
                        const auto& points = entry->second;
                        const auto same = note.amplitudeEnvelope.size() == points.size()
                            && std::equal(note.amplitudeEnvelope.begin(),
                                          note.amplitudeEnvelope.end(), points.begin(),
                                [](const auto& left, const auto& right)
                                {
                                    return std::abs(left.timeSeconds - right.timeSeconds) <= 1.0e-7
                                        && std::abs(left.gainDb - right.gainDb) <= 1.0e-4f
                                        && left.linearToNext == right.linearToNext
                                        && left.nativeSeamAnchor == right.nativeSeamAnchor;
                                });
                        if (same) continue;
                        if (!changed) pushUndoLocked();
                        note.amplitudeEnvelope = points;
                        changed = true;
                    }
    }
    if (changed) sendChangeMessage();
    return changed;
}


void ProjectModel::setNoteRobustPitchCurve(const juce::String& noteId, bool enabled)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == noteId && note.robustPitchCurve != enabled)
                    {
                        pushUndoLocked();
                        note.robustPitchCurve = enabled;
                        changed = true;
                        break;
                    }
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::setDiffSingerOptions(const juce::String& trackId,
    const juce::String& language, const juce::String& speaker)
{
    const juce::ScopedLock guard(lock);
    for (auto& track : project.tracks)
        if (track.id == trackId && (track.diffSingerLanguage != language || track.diffSingerSpeaker != speaker))
        {
            pushUndoLocked();
            track.diffSingerLanguage = language;
            track.diffSingerSpeaker = speaker;
            sendChangeMessage();
            break;
        }
}

bool ProjectModel::applyDiffSingerPronunciation(std::uint64_t expectedRevision, const juce::String& trackId,
    const std::vector<std::pair<juce::String, juce::String>>& readings, const juce::String& dictionary)
{
    const juce::ScopedLock guard(lock);
    if (revision != expectedRevision) return false;
    auto track = std::find_if(project.tracks.begin(), project.tracks.end(),
        [&](const auto& item) { return item.id == trackId && trackIsDiffSinger(item); });
    if (track == project.tracks.end()) return false;
    std::vector<std::pair<NoteData*, juce::String>> changes;
    for (const auto& [id, value] : readings)
    {
        NoteData* found = nullptr;
        for (auto& clip : track->clips) for (auto& note : clip.notes) if (note.id == id) found = &note;
        if (found == nullptr) return false;
        if (found->diffSingerPronunciation != value.trim()) changes.emplace_back(found, value.trim());
    }
    if (changes.empty() && track->diffSingerDictionary == dictionary) return true;
    pushUndoLocked();
    if (track->diffSingerDictionary != dictionary)
        for (auto& clip : track->clips) for (auto& note : clip.notes) note.diffSingerTiming.clear();
    track->diffSingerDictionary = dictionary;
    for (const auto& [note, text] : changes)
    { note->diffSingerPronunciation = text; note->diffSingerTiming.clear(); }
    sendChangeMessage();
    return true;
}

bool ProjectModel::applyDiffSingerTiming(std::uint64_t expectedRevision,
    const std::vector<std::pair<juce::String, juce::String>>& timings)
{
    const juce::ScopedLock guard(lock);
    if (revision != expectedRevision || timings.empty()) return false;
    std::vector<std::pair<NoteData*, juce::String>> changes;
    for (const auto& [id, text] : timings)
    {
        if (text.isNotEmpty())
        {
            const auto value = juce::JSON::parse(text);
            if (!value.isObject() || value["context"].toString().isEmpty()
                || !value["tokens"].isArray() || !value["starts"].isArray()
                || value["tokens"].size() != value["starts"].size()) return false;
            for (const auto& at : *value["starts"].getArray())
                if (!at.isVoid() && (!(at.isDouble() || at.isInt() || at.isInt64())
                    || !std::isfinite((double) at))) return false;
        }
        NoteData* found = nullptr;
        for (auto& track : project.tracks) if (trackIsDiffSinger(track))
            for (auto& clip : track.clips) for (auto& note : clip.notes)
                if (note.id == id) found = &note;
        if (found == nullptr) return false;
        if (found->diffSingerTiming != text) changes.emplace_back(found, text);
    }
    if (changes.empty()) return true;
    pushUndoLocked();
    for (const auto& [note, text] : changes) note->diffSingerTiming = text;
    sendChangeMessage();
    return true;
}

bool ProjectModel::restoreDiffSingerPitchPoint(std::uint64_t expectedRevision,
    const juce::String& noteId, const std::vector<PitchCurveEditPoint>& displayedPoints, int index)
{
    const juce::ScopedLock guard(lock);
    if (revision != expectedRevision) return false;
    for (const auto& p : displayedPoints)
        if (!std::isfinite(p.timeSeconds) || !std::isfinite(p.targetMidi)) return false;
    for (auto& track : project.tracks) if (trackIsDiffSinger(track))
        for (auto& clip : track.clips) for (auto& note : clip.notes)
            if (note.id == noteId)
            {
                const auto restored = restoredDiffSingerPitchPoint(note, displayedPoints, index);
                return !restored.empty() && setNotePitchCurve(noteId, restored, true);
            }
    return false;
}

bool ProjectModel::applyDiffSingerPitch(std::uint64_t expectedRevision,
    const std::vector<std::pair<juce::String, std::vector<PitchCurveEditPoint>>>& curves,
    bool preserveOutsideNoteHandles)
{
    const juce::ScopedLock guard(lock);
    if (revision != expectedRevision || curves.empty()) return false;
    for (const auto& [id, points] : curves)
    {
        bool found = false;
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes) if (note.id == id) found = true;
        if (!found || points.empty()) return false;
        for (const auto& p : points)
            if (!std::isfinite(p.timeSeconds) || !std::isfinite(p.targetMidi)
                || p.targetMidi < 0 || p.targetMidi > 127) return false;
    }
    pushUndoLocked();
    const juce::ScopedValueSetter<bool> grouping(suppressNestedUndo, true);
    for (const auto& [id, points] : curves)
    {
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == id) { note.vibratoEnabled = false; note.utauAutoPitchTransition = false; }
        setNotePitchCurve(id, points, true);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (note.id == id)
                    {
                        note.diffSingerPitchReference = note.pitchControlPoints;
                        note.diffSingerPitchReferenceFromSavedPitch = false;
                        note.pitchControlPoints = compactDiffSingerPitchHandles(note.pitchControlPoints,
                            note.diffSingerPitchReference, note.durationSeconds, preserveOutsideNoteHandles);
                    }
    }
    return true;
}

bool ProjectModel::setDiffSingerPitchOffset(const juce::String& noteId,
                                            std::vector<PitchCurveEditPoint> points)
{
    for (const auto& p : points)
        if (!std::isfinite(p.timeSeconds) || !std::isfinite(p.targetMidi)) return false;
    const juce::ScopedLock guard(lock);
    for (auto& track : project.tracks) if (trackIsDiffSinger(track))
        for (auto& clip : track.clips) for (auto& note : clip.notes) if (note.id == noteId)
        {
            for (auto& p : points)
            {
                p.timeSeconds = juce::jlimit(0.0, note.durationSeconds, p.timeSeconds);
                p.targetMidi = juce::jlimit(-127.0f, 127.0f, p.targetMidi);
                p.shape = PitchCurveShape::linear;
                p.diffSingerRestoreSupport = false;
            }
            std::stable_sort(points.begin(), points.end(), [](const auto& a, const auto& b){return a.timeSeconds<b.timeSeconds;});
            std::vector<PitchCurveEditPoint> clean;
            for (const auto& p : points)
                if (!clean.empty() && std::abs(p.timeSeconds-clean.back().timeSeconds)<1e-7) clean.back()=p;
                else clean.push_back(p);
            if (clean.size()<=2 && std::all_of(clean.begin(),clean.end(),[](const auto& p){return std::abs(p.targetMidi)<1e-6f;})) clean.clear();
            const auto& old=note.diffSingerPitchOffset;
            if (old.size()==clean.size() && std::equal(old.begin(),old.end(),clean.begin(),[](const auto& a,const auto& b){
                return a.timeSeconds==b.timeSeconds && a.targetMidi==b.targetMidi;})) return true;
            pushUndoLocked();note.diffSingerPitchOffset=std::move(clean);sendChangeMessage();return true;
        }
    return false;
}

bool ProjectModel::setNotePitchCurve(const juce::String& noteId,
                                     std::vector<PitchCurveEditPoint> points,
                                     bool storeControlPoints)
{
    if (points.empty()) return false;
    std::stable_sort(points.begin(), points.end(), [](const auto& left, const auto& right)
    {
        return left.timeSeconds < right.timeSeconds;
    });
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (note.id != noteId || note.nativeUnpitched) continue;
                    for (auto& point : points)
                    {
                        // Handles reach before the note, and a UST bend runs
                        // on past its end.  Held to the note's own span, every
                        // point past the end was pulled back to it whenever any
                        // one point of the note was touched -- moving points
                        // nobody had moved, and reshaping the note's tail.
                        const auto minimumTime = storeControlPoints ? -30.0 : 0.0;
                        const auto maximumTime = storeControlPoints
                            ? note.durationSeconds + 30.0 : note.durationSeconds;
                        point.timeSeconds = juce::jlimit(minimumTime, maximumTime,
                                                         point.timeSeconds);
                        point.targetMidi = juce::jlimit(0.0f, 127.0f, point.targetMidi);
                    }
                    pushUndoLocked();
                    changed = true;
                    if (trackShowsAllNativeRegions(track)) note.nativePitchHandlesPlaced = true;
                    if (storeControlPoints) note.pitchControlPoints = points;
                    else note.pitchControlPoints.clear();

                    // User-created notes initially contain only two endpoints.
                    // Densify them before drawing so a freehand edit has the
                    // same 5 ms precision as imported Melodyne/FCPE contours.
                    auto needsDensifying = note.contour.size() < 2;
                    for (std::size_t index = 1; index < note.contour.size(); ++index)
                        needsDensifying = needsDensifying
                            || note.contour[index].timeSeconds
                                - note.contour[index - 1].timeSeconds > 0.0075;
                    if (needsDensifying)
                    {
                        const auto original = note.contour;
                        const auto evaluate = [&](double time)
                        {
                            PitchPoint result;
                            result.timeSeconds = time;
                            if (original.empty()) return result;
                            const auto right = std::lower_bound(original.begin(), original.end(), time,
                                [](const PitchPoint& point, double value)
                                {
                                    return point.timeSeconds < value;
                                });
                            const auto rightIndex = static_cast<std::size_t>(right == original.end()
                                ? original.size() - 1 : right - original.begin());
                            const auto leftIndex = rightIndex > 0
                                && original[rightIndex].timeSeconds > time ? rightIndex - 1 : rightIndex;
                            const auto& left = original[leftIndex];
                            const auto& next = original[rightIndex];
                            const auto amount = next.timeSeconds > left.timeSeconds
                                ? static_cast<float>(juce::jlimit(0.0, 1.0,
                                    (time - left.timeSeconds)
                                        / (next.timeSeconds - left.timeSeconds))) : 0.0f;
                            result.relativeCents = left.relativeCents
                                + (next.relativeCents - left.relativeCents) * amount;
                            result.withoutVibratoCents = left.withoutVibratoCents
                                + (next.withoutVibratoCents - left.withoutVibratoCents) * amount;
                            result.voiced = left.voiced && next.voiced;
                            if (left.hasManualTarget && next.hasManualTarget)
                            {
                                result.hasManualTarget = true;
                                result.manualTargetCents = left.manualTargetCents
                                    + (next.manualTargetCents - left.manualTargetCents) * amount;
                            }
                            return result;
                        };
                        note.contour.clear();
                        for (double time = 0.0; time < note.durationSeconds; time += 0.005)
                            note.contour.push_back(evaluate(time));
                        note.contour.push_back(evaluate(note.durationSeconds));
                    }

                    const auto firstTime = points.front().timeSeconds;
                    const auto lastTime = points.back().timeSeconds;
                    const auto targetAt = [&](double time)
                    {
                        return evaluatePitchCurve(points, time);
                    };
                    if (points.size() == 1 && !note.contour.empty())
                    {
                        auto nearest = std::min_element(note.contour.begin(), note.contour.end(),
                            [&](const auto& left, const auto& right)
                            {
                                return std::abs(left.timeSeconds - firstTime)
                                    < std::abs(right.timeSeconds - firstTime);
                            });
                        nearest->manualTargetCents =
                            (points.front().targetMidi - note.midiNote) * 100.0f;
                        nearest->hasManualTarget = true;
                    }
                    else
                    {
                        // Control points describe the whole note, so they apply
                        // to all of it.  Anchors get trimmed away from the ends
                        // where a note abuts its neighbour, and writing only
                        // between the outermost anchors left the contour holding
                        // its old value beyond them - a step down at the first
                        // anchor and back up at the last, which is what showed
                        // up as a break across the consonant even though the
                        // anchors themselves were continuous.  Outside the
                        // anchor span evaluatePitchCurve holds the end value,
                        // matching what encodePitchbend sends.
                        //
                        // A freehand or line stroke is a local edit and stays
                        // confined to the stretch it covers, so that it cannot
                        // wipe measured pitch elsewhere in the note.
                        // Native UV samples retain the authored guide too, but
                        // keep voiced=false: the guide never creates source F0.
                        for (auto& point : note.contour)
                            if ((point.voiced || trackShowsAllNativeRegions(track))
                                && (storeControlPoints
                                    || (point.timeSeconds >= firstTime - 1.0e-7
                                        && point.timeSeconds <= lastTime + 1.0e-7)))
                            {
                                point.manualTargetCents =
                                    (targetAt(point.timeSeconds) - note.midiNote) * 100.0f;
                                point.hasManualTarget = true;
                            }
                    }
                    break;
                }
    }
    if (changed) sendChangeMessage();
    return changed;
}

ProjectModel::PlannedNote ProjectModel::plannedNoteFor(
    double startSeconds, double requestedSeconds, double clipSeconds,
    const std::vector<NoteSpan>& occupied)
{
    // Anything shorter than this is widened to the model's own floor below,
    // which would put the overlap straight back in.
    constexpr auto shortest = 0.01;
    constexpr auto epsilon = 1.0e-9;

    PlannedNote planned;
    if (startSeconds < -epsilon) return planned;
    // Past the end of the clip there is no room.  Clamping instead is what
    // used to stack notes on the final instant.
    if (startSeconds > clipSeconds - shortest + epsilon) return planned;

    auto room = std::min(requestedSeconds, clipSeconds - startSeconds);
    for (const auto& span : occupied)
    {
        const auto end = span.startSeconds + span.durationSeconds;
        // The moment asked for is inside a note that is already there.
        if (span.startSeconds <= startSeconds + epsilon
            && startSeconds < end - epsilon)
            return planned;
        // A note begins further along: the new one stops where it starts.
        if (span.startSeconds > startSeconds)
            room = std::min(room, span.startSeconds - startSeconds);
    }
    if (room < shortest - epsilon) return planned;

    planned.create = true;
    planned.startSeconds = startSeconds;
    planned.durationSeconds = room;
    return planned;
}

namespace
{
// How far into a clip a note may reach.  A clip with a recording behind it is
// as long as its audio and that is the end of it; one composed here is only a
// span of the timeline, so a note may push it out.
double writableEndOf(const ClipData& clip, double startSeconds, double duration)
{
    if (clip.sourceFile != juce::File()) return clip.durationSeconds;
    return std::max(clip.durationSeconds, startSeconds + std::max(0.0, duration));
}
}

bool ProjectModel::canAddEmptyTuningClip(const juce::String& trackId, double startSeconds) const
{
    if (!std::isfinite(startSeconds) || startSeconds < 0.0) return false;
    const juce::ScopedLock guard(lock);
    for (const auto& track : project.tracks)
        if (track.id == trackId && !track.accompaniment)
            return std::none_of(track.clips.begin(),track.clips.end(),[&](const auto& clip)
            { return startSeconds >= clip.startSeconds && startSeconds < clip.startSeconds+clip.durationSeconds; });
    return false;
}

juce::String ProjectModel::addEmptyTuningClip(const juce::String& trackId, double startSeconds, double durationSeconds)
{
    if (!std::isfinite(durationSeconds) || durationSeconds < 0.01 || !std::isfinite(startSeconds+durationSeconds)) return {};
    juce::String created;
    {
        const juce::ScopedLock guard(lock);
        if (!canAddEmptyTuningClip(trackId,startSeconds)) return {};
        for (auto& track : project.tracks) if (track.id == trackId)
        {
            pushUndoLocked();
            track.compose = true;
            ClipData clip;clip.id=created=makeId("clip");clip.startSeconds=startSeconds;
            clip.durationSeconds=clip.sourceDurationSeconds=durationSeconds;
            track.clips.push_back(std::move(clip));
            std::stable_sort(track.clips.begin(),track.clips.end(),[](const auto& a,const auto& b){return a.startSeconds<b.startSeconds;});
            break;
        }
    }
    if (created.isNotEmpty()) sendChangeMessage();
    return created;
}

juce::String ProjectModel::addClip(const juce::String& trackId,
                                   double startSeconds, double durationSeconds)
{
    juce::String created;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            if (track.id == trackId && track.compose)
            {
                pushUndoLocked();
                ClipData clip;
                clip.id = makeId("clip");
                clip.startSeconds = std::max(0.0, startSeconds);
                clip.durationSeconds = std::max(0.01, durationSeconds);
                clip.sourceDurationSeconds = clip.durationSeconds;
                created = clip.id;
                track.clips.push_back(std::move(clip));
                std::stable_sort(track.clips.begin(), track.clips.end(),
                    [](const auto& left, const auto& right)
                    { return left.startSeconds < right.startSeconds; });
                break;
            }
    }
    if (created.isNotEmpty()) sendChangeMessage();
    return created;
}

juce::String ProjectModel::addNoteLocked(ClipData& destination,
                                         const PlannedNote& planned, float midiNote)
{
    pushUndoLocked();
    NoteData note;
    for (const auto& track : project.tracks) for (const auto& clip : track.clips)
        if (&clip == &destination) note.utauFlagCurveEnabled = trackIsDiffSinger(track);
    note.id = makeId("note");
    note.startSeconds = planned.startSeconds;
    note.durationSeconds = planned.durationSeconds;
    note.consonantSeconds = std::min(0.04, note.durationSeconds * 0.3);
    note.midiNote = juce::jlimit(0.0f, 127.0f, midiNote);
    note.sourceMidiCenter = note.midiNote;
    note.contour.push_back({ 0.0, 0.0f, 0.0f, true });
    note.contour.push_back({ note.durationSeconds, 0.0f, 0.0f, true });
    const auto id = note.id;
    const auto reach = note.startSeconds + note.durationSeconds;
    destination.notes.push_back(std::move(note));
    if (destination.sourceFile == juce::File() && destination.durationSeconds < reach)
    {
        destination.durationSeconds = reach;
        destination.sourceDurationSeconds = reach;
    }
    std::stable_sort(destination.notes.begin(), destination.notes.end(),
        [](const auto& left, const auto& right) { return left.startSeconds < right.startSeconds; });
    return id;
}

juce::String ProjectModel::insertPrefixNote(const juce::String& noteId,
                                           double targetOverlapSeconds)
{
    juce::String created;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
            {
                const auto found = std::find_if(clip.notes.begin(), clip.notes.end(),
                    [&noteId](const auto& note) { return note.id == noteId; });
                if (found == clip.notes.end()) continue;
                if (found->startSeconds <= 1.0e-9) return {};
                pushUndoLocked();
                // Its vowel from its own beat, so the consonant is heard up to
                // it.  The overlap it had goes back in with the pin, since the
                // two are pinned together.
                found->utauPreutteranceOverrideEnabled = true;
                found->utauPreutteranceSeconds = 0.0;
                found->utauOverlapOverrideEnabled = true;
                found->utauOverlapSeconds = std::isfinite(targetOverlapSeconds)
                    ? targetOverlapSeconds : 0.0;
                NoteData prefix;
                prefix.id = makeId("note");
                prefix.startSeconds = found->startSeconds;
                prefix.durationSeconds = 0.0;
                prefix.consonantSeconds = 0.0;
                prefix.midiNote = found->midiNote;
                prefix.sourceMidiCenter = found->midiNote;
                prefix.label = found->label;
                prefix.contour.push_back({ 0.0, 0.0f, 0.0f, true });
                prefix.contour.push_back({ 0.0, 0.0f, 0.0f, true });
                created = prefix.id;
                clip.notes.insert(found, std::move(prefix));
                break;
            }
    }
    if (created.isNotEmpty()) sendChangeMessage();
    return created;
}

void ProjectModel::flattenNotePitch(const std::vector<juce::String>& noteIds, bool snapNativeToSemitone)
{
    if (noteIds.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (std::find(noteIds.begin(), noteIds.end(), note.id)
                        == noteIds.end())
                        continue;
                    if (note.nativeUnpitched) continue;
                    if (!changed) pushUndoLocked();
                    changed = true;

                    if (snapNativeToSemitone && trackShowsAllNativeRegions(track))
                        note.midiNote = juce::jlimit(0.0f, 127.0f, std::round(note.midiNote));
                    if (trackShowsAllNativeRegions(track))
                    {
                        note.nativeIndependentPitch = true;
                        note.nativePitchHandlesPlaced = false;
                        note.utauAutoPitchTransition = false;
                        note.vibratoEnabled = false;
                    }
                    for (auto& point : note.contour)
                    {
                        // Every frame, not only the voiced ones.  Where nothing
                        // was detected the note should move with the rest
                        // rather than keep whatever it was holding.
                        point.manualTargetCents = 0.0f;
                        point.hasManualTarget = true;
                    }
                    // Drift and modulation reshape the measured curve, and
                    // there is no longer a curve to reshape.
                    note.drift = 0.0f;
                    note.modulation = 0.0f;
                    note.pitchControlPoints = {
                        { 0.0, note.midiNote },
                        { note.durationSeconds, note.midiNote },
                    };
                }
    }
    if (changed) sendChangeMessage();
}

bool ProjectModel::restoreNativeSourcePitch(const std::vector<juce::String>& noteIds)
{
    bool changed = false;
    {
        const juce::ScopedLock guard(lock);
        std::vector<NoteData*> targets;
        std::vector<juce::String> restoredIds;
        for (auto& track : project.tracks)
        {
            if (!trackShowsAllNativeRegions(track)) continue;
            for (auto& clip : track.clips) for (auto& note : clip.notes)
                if (std::find(noteIds.begin(), noteIds.end(), note.id) != noteIds.end()
                    && !note.nativeUnpitched && std::isfinite(note.sourceMidiCenter) && note.sourceMidiCenter >= 0.0f
                    && !note.contour.empty())
                { targets.push_back(&note); restoredIds.push_back(note.id); }
        }
        if (targets.empty()) return false;
        const auto includes = [&](const juce::String& id) {
            return std::find(restoredIds.begin(), restoredIds.end(), id) != restoredIds.end();
        };
        const auto edited = [](const NoteData* note) {
            return note->midiNote != note->sourceMidiCenter || note->drift != 1.0f
                || note->modulation != 1.0f || !note->pitchControlPoints.empty()
                || note->vibratoEnabled || note->vibratoRealLine || note->robustPitchCurve
                || note->connectedToPrevious || note->connectedToNext || note->utauAutoPitchTransition
                || std::any_of(note->contour.begin(), note->contour.end(),
                    [](const auto& point) { return point.hasManualTarget; });
        };
        changed = std::any_of(targets.begin(), targets.end(), edited)
            || std::any_of(project.nativeConnections.begin(), project.nativeConnections.end(),
                [&](const auto& connection) { return includes(connection.leftNoteId) || includes(connection.rightNoteId); });
        if (!changed) return false;
        pushUndoLocked();
        // Unlink pitch glides at both endpoints, including older projects with
        // only compatibility flags.  A synthetic glide must not reshape restored F0.
        for (auto& track : project.tracks)
        {
            if (!trackShowsAllNativeRegions(track)) continue;
            struct Placed { NoteData* note; ClipData* clip; double start; };
            std::vector<Placed> ordered;
            for (auto& clip : track.clips) for (auto& note : clip.notes)
                ordered.push_back({&note, &clip, clip.startSeconds + note.startSeconds});
            std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.start < b.start; });
            for (std::size_t i = 0; i < ordered.size(); ++i)
            {
                if (!includes(ordered[i].note->id)) continue;
                auto& note = *ordered[i].note;
                if (note.connectedToPrevious && i > 0)
                { ordered[i-1].note->connectedToNext = false; ordered[i-1].clip->glideConnectedToNext = false; }
                if (note.connectedToNext && i+1 < ordered.size())
                { ordered[i+1].note->connectedToPrevious = false; ordered[i+1].clip->glideConnectedFromPrevious = false; }
                if (note.connectedToPrevious) ordered[i].clip->glideConnectedFromPrevious = false;
                if (note.connectedToNext) ordered[i].clip->glideConnectedToNext = false;
            }
        }
        for (auto& track : project.tracks) for (auto& clip : track.clips) for (auto& note : clip.notes)
            for (const auto& connection : project.nativeConnections)
                if (includes(connection.leftNoteId) || includes(connection.rightNoteId))
                {
                    if (note.id == connection.leftNoteId) { note.connectedToNext = false; clip.glideConnectedToNext = false; }
                    if (note.id == connection.rightNoteId) { note.connectedToPrevious = false; clip.glideConnectedFromPrevious = false; }
                }
        std::erase_if(project.nativeConnections, [&](const auto& connection) {
            return includes(connection.leftNoteId) || includes(connection.rightNoteId);
        });
        for (auto* note : targets)
        {
            note->midiNote = note->sourceMidiCenter;
            note->drift = note->modulation = 1.0f;
            note->pitchControlPoints.clear();
            note->vibratoEnabled = note->vibratoRealLine = false;
            note->robustPitchCurve = false;
            note->connectedToPrevious = note->connectedToNext = false;
            note->utauAutoPitchTransition = false;
            for (auto& point : note->contour)
            { point.hasManualTarget = false; point.manualTargetCents = 0.0f; }
        }
    }
    if (changed) sendChangeMessage();
    return changed;
}

double ProjectModel::firstFreeStartFrom(double startSeconds,
                                        const std::vector<NoteSpan>& occupied)
{
    constexpr auto epsilon = 1.0e-9;
    auto start = startSeconds;
    // One pass per note at most: each step lands on the end of a note that
    // covered the last position, and a note cannot be crossed twice.  The
    // bound also keeps a zero-length note from spinning here.
    for (std::size_t step = 0; step <= occupied.size(); ++step)
    {
        auto moved = false;
        for (const auto& span : occupied)
        {
            const auto end = span.startSeconds + span.durationSeconds;
            if (span.startSeconds <= start + epsilon && start < end - epsilon)
            {
                start = end;
                moved = true;
            }
        }
        if (!moved) break;
    }
    return start;
}

juce::String ProjectModel::addNoteFrom(const juce::String& preferredClipId,
                                       double absoluteFromSeconds,
                                       double maximumDuration, float midiNote)
{
    juce::String created;
    {
        const juce::ScopedLock guard(lock);
        ClipData* destination = nullptr;
        for (auto& track : project.tracks)
            if (track.compose)
                for (auto& clip : track.clips)
                {
                    if (clip.id == preferredClipId) destination = &clip;
                    if (destination == nullptr
                        && absoluteFromSeconds >= clip.startSeconds
                        && absoluteFromSeconds <= clip.startSeconds + clip.durationSeconds)
                        destination = &clip;
                }
        if (destination != nullptr)
        {
            std::vector<NoteSpan> occupied;
            occupied.reserve(destination->notes.size());
            for (const auto& existing : destination->notes)
                occupied.push_back({ existing.startSeconds, existing.durationSeconds });
            const auto from = firstFreeStartFrom(
                absoluteFromSeconds - destination->startSeconds, occupied);
            const auto planned = plannedNoteFor(from, maximumDuration,
                                                writableEndOf(*destination, from,
                                                              maximumDuration),
                                                occupied);
            if (planned.create)
                created = addNoteLocked(*destination, planned, midiNote);
        }
    }
    if (created.isNotEmpty()) sendChangeMessage();
    return created;
}

juce::String ProjectModel::addNote(const juce::String& preferredClipId,
                                   double absoluteStart, double duration, float midiNote)
{
    juce::String created;
    {
        const juce::ScopedLock guard(lock);
        ClipData* destination = nullptr;
        for (auto& track : project.tracks)
            if (track.compose)
                for (auto& clip : track.clips)
                {
                    if (clip.id == preferredClipId) destination = &clip;
                    if (destination == nullptr
                        && absoluteStart >= clip.startSeconds
                        && absoluteStart <= clip.startSeconds + clip.durationSeconds)
                        destination = &clip;
                }
        if (destination != nullptr)
        {
            std::vector<NoteSpan> occupied;
            occupied.reserve(destination->notes.size());
            for (const auto& existing : destination->notes)
                occupied.push_back({ existing.startSeconds, existing.durationSeconds });
            const auto local = absoluteStart - destination->startSeconds;
            const auto planned = plannedNoteFor(local, duration,
                                                writableEndOf(*destination, local, duration),
                                                occupied);
            if (!planned.create) return {};
            created = addNoteLocked(*destination, planned, midiNote);
        }
    }
    if (created.isNotEmpty()) sendChangeMessage();
    return created;
}

void ProjectModel::removeNote(const juce::String& noteId)
{
    removeNotes({ noteId });
}

std::vector<std::size_t> ProjectModel::regionsToImport(
    const std::vector<RegionSpan>& regions)
{
    constexpr auto epsilon = 1.0e-9;
    // Longest first, so the row that covers the most of the syllable is the
    // one kept; ties go to whichever came first in the file, which keeps the
    // result stable rather than dependent on the sort.
    std::vector<std::size_t> order(regions.size());
    for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
    std::stable_sort(order.begin(), order.end(),
        [&regions](std::size_t left, std::size_t right)
        {
            const auto leftLength = regions[left].endSeconds - regions[left].startSeconds;
            const auto rightLength = regions[right].endSeconds - regions[right].startSeconds;
            // Two rows of the same span at different offsets do not subtract
            // to bitwise equal lengths -- 0.3-0.1 and 0.4-0.2 differ in the
            // last bits -- and which alias you get must not hinge on that.
            // Equal within a nanosecond counts as equal, and stable_sort then
            // leaves the earlier row first.
            if (std::abs(leftLength - rightLength) <= epsilon) return false;
            return leftLength > rightLength;
        });

    std::vector<std::size_t> kept;
    for (const auto candidate : order)
    {
        const auto& region = regions[candidate];
        if (region.endSeconds - region.startSeconds < epsilon) continue;
        const auto clashes = std::any_of(kept.begin(), kept.end(),
            [&](std::size_t taken)
            {
                return region.startSeconds < regions[taken].endSeconds - epsilon
                    && regions[taken].startSeconds < region.endSeconds - epsilon;
            });
        if (!clashes) kept.push_back(candidate);
    }
    // Back into the order the file gave them, so the notes read left to right.
    std::sort(kept.begin(), kept.end());
    return kept;
}

bool ProjectModel::clipIsRecording(const ClipData& clip)
{
    if (clip.sourceFile == juce::File()) return false;
    return !clip.sourceFile.hasFileExtension("ust;mid;midi;mpd;hjpx;hspx");
}

void ProjectModel::dropEmptyRecordedClipsLocked()
{
    for (auto& track : project.tracks)
        std::erase_if(track.clips, [](const ClipData& clip)
        {
            return clip.notes.empty() && clipIsRecording(clip);
        });
}

void ProjectModel::removeNotes(const std::vector<juce::String>& noteIds)
{
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (const auto& track : project.tracks)
            for (const auto& clip : track.clips)
                for (const auto& note : clip.notes)
                    changed = changed || includes(note.id);
        if (!changed) return;
        pushUndoLocked();
        for (auto& track : project.tracks)
        {
            if(trackShowsAllNativeRegions(track))
                for(auto& clip:track.clips)rememberNativeTrimSources(clip);
            struct Positioned { NoteData* note; double start; };
            std::vector<Positioned> ordered;
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    ordered.push_back({ &note, clip.startSeconds + note.startSeconds });
            std::stable_sort(ordered.begin(), ordered.end(),
                [](const auto& left, const auto& right) { return left.start < right.start; });
            for (std::size_t index = 0; index < ordered.size(); ++index)
                if (includes(ordered[index].note->id))
                {
                    if (index > 0 && !includes(ordered[index - 1].note->id))
                        ordered[index - 1].note->connectedToNext = false;
                    if (index + 1 < ordered.size() && !includes(ordered[index + 1].note->id))
                        ordered[index + 1].note->connectedToPrevious = false;
                }
            if (trackShowsAllNativeRegions(track))
            {
                std::vector<ClipData> retained;
                for (const auto& original : track.clips)
                {
                    auto parts = removeNativeUnpitchedAudio(original, noteIds);
                    for (std::size_t i = 0; i < parts.size(); ++i)
                    {
                        parts[i].id = i == 0 ? original.id : makeId("clip");
                        retained.push_back(std::move(parts[i]));
                    }
                }
                track.clips = std::move(retained);
            }
            for (auto& clip : track.clips)
                clip.notes.erase(std::remove_if(clip.notes.begin(), clip.notes.end(),
                    [&](const auto& note) { return includes(note.id); }), clip.notes.end());
        }
        std::erase_if(project.nativeConnections, [&](const auto& c)
        { return includes(c.leftNoteId) || includes(c.rightNoteId); });
        dropEmptyRecordedClipsLocked();
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::toggleNoteConnection(const juce::String& noteId)
{
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        for (auto& track : project.tracks)
        {
            struct Positioned { NoteData* note; double start; };
            std::vector<Positioned> ordered;
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    ordered.push_back({ &note, clip.startSeconds + note.startSeconds });
            std::stable_sort(ordered.begin(), ordered.end(),
                [](const auto& left, const auto& right) { return left.start < right.start; });
            for (std::size_t index = 1; index < ordered.size(); ++index)
                if (ordered[index].note->id == noteId)
                {
                    pushUndoLocked();
                    const auto connected = ordered[index].note->connectedToPrevious
                        && ordered[index - 1].note->connectedToNext;
                    ordered[index].note->connectedToPrevious = !connected;
                    ordered[index - 1].note->connectedToNext = !connected;
                    if (connected)
                    {
                        std::erase_if(project.nativeConnections,
                            [&](const auto& connection)
                            {
                                return connection.leftNoteId == ordered[index - 1].note->id
                                    && connection.rightNoteId == ordered[index].note->id;
                            });
                    }
                    else
                    {
                        std::erase_if(project.nativeConnections,
                            [&](const auto& connection)
                            {
                                return connection.leftNoteId == ordered[index - 1].note->id
                                    || connection.rightNoteId == ordered[index - 1].note->id
                                    || connection.leftNoteId == ordered[index].note->id
                                    || connection.rightNoteId == ordered[index].note->id;
                            });
                        project.nativeConnections.push_back({
                            makeId("connection"), ordered[index - 1].note->id,
                            ordered[index].note->id, "pitch-and-amplitude",
                            (ordered[index - 1].start + ordered[index].start) * 0.5,
                            {}, {} });
                    }
                    changed = true;
                    break;
                }
            if (changed) break;
        }
    }
    if (changed) sendChangeMessage();
}


bool ProjectModel::setNativeAudioOverlap(const juce::String& trackId,bool enabled)
{
    {
        const juce::ScopedLock guard(lock);
        const auto found=std::find_if(project.tracks.begin(),project.tracks.end(),[&](const auto& t){return t.id==trackId;});
        if(found==project.tracks.end()||!trackShowsAllNativeRegions(*found))return false;
        auto next=*found;const auto cropped=!enabled&&removeNativeAudioOverlaps(next);
        if(!cropped&&next.allowNativeAudioOverlap==enabled)return false;
        // Cropping an incoming head also removes its old synthetic join.
        // The outgoing endpoint must not retain a compatibility glide flag.
        std::vector<juce::String> cutHeads;
        for(const auto& oldClip:found->clips)for(const auto& oldNote:oldClip.notes)
        {
            bool retained=false;
            for(const auto& newClip:next.clips)for(const auto& newNote:newClip.notes)if(newNote.id==oldNote.id)
            {retained=true;if(newNote.durationSeconds<oldNote.durationSeconds-1.e-9)cutHeads.push_back(oldNote.id);}
            if(!retained)cutHeads.push_back(oldNote.id);
        }
        next.allowNativeAudioOverlap=enabled;pushUndoLocked();*found=std::move(next);
        const auto cut=[&](const auto& id){return std::find(cutHeads.begin(),cutHeads.end(),id)!=cutHeads.end();};
        for(const auto& link:project.nativeConnections)if(cut(link.rightNoteId)||cut(link.leftNoteId))
            for(auto& t:project.tracks)for(auto& c:t.clips)for(auto& n:c.notes)
            {
                if(n.id==link.leftNoteId){n.connectedToNext=false;c.glideConnectedToNext=false;}
                if(n.id==link.rightNoteId){n.connectedToPrevious=false;c.glideConnectedFromPrevious=false;}
            }
        std::erase_if(project.nativeConnections,[&](const auto& c){return cut(c.leftNoteId)||cut(c.rightNoteId);});
    }
    sendChangeMessage();return true;
}

bool ProjectModel::linkNativeAudio(const std::vector<juce::String>& ids)
{
    {
        const juce::ScopedLock guard(lock);
        if(!nativeAudioLinkAvailable(project,ids))return false;
        auto updated=project;
        const auto selected=[&](const auto& n){return std::find(ids.begin(),ids.end(),n.id)!=ids.end();};
        for(auto& track:updated.tracks)
        {
            std::vector<ClipData> kept,chosen;bool affected=false;
            for(const auto& clip:track.clips)
            {
                if(std::none_of(clip.notes.begin(),clip.notes.end(),selected)){kept.push_back(clip);continue;}
                affected=true;auto parts=disconnectedNativeClip(clip,ids);if(!parts)return false;
                for(auto part:*parts)
                {
                    part.id=makeId("clip");
                    if(std::any_of(part.notes.begin(),part.notes.end(),selected))chosen.push_back(std::move(part));
                    else kept.push_back(std::move(part));
                }
            }
            if(!affected)continue;
            if(chosen.size()<2)return false;
            auto linked=assembledLinkedAudio(chosen);linked.id=makeId("clip");kept.push_back(std::move(linked));
            track.clips=std::move(kept);
        }
        pushUndoLocked();project=std::move(updated);
    }
    sendChangeMessage();return true;
}

bool ProjectModel::disconnectNativeAudio(const std::vector<juce::String>& ids)
{
    {
        const juce::ScopedLock guard(lock);
        if (!nativeAudioDisconnectAvailable(project,ids)) return false;
        // Prepare the whole edit before touching live data: one undo step, and
        // an invalid source/cut cannot leave half a selection disconnected.
        auto updated=project;
        const auto selected=[&](const auto& id){return std::find(ids.begin(),ids.end(),id)!=ids.end();};
        for (auto& track : updated.tracks)
        {
            if (!trackShowsAllNativeRegions(track)) continue;
            const auto envelopes = nativeSharedEnvelopes(track);
            for (auto& clip : track.clips)
                if (std::any_of(clip.notes.begin(), clip.notes.end(), [&](const auto& n) { return selected(n.id); }))
                    for (auto& note : clip.notes) materializeNativeSharedEnvelope(note, envelopes);
            struct Placed { NoteData* note; ClipData* clip; double start; };
            std::vector<Placed> ordered;
            for (auto& clip : track.clips) for (auto& note : clip.notes)
                ordered.push_back({&note,&clip,clip.startSeconds+note.startSeconds});
            std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.start<b.start;});
            for (std::size_t i=1;i<ordered.size();++i)
                if (selected(ordered[i-1].note->id) || selected(ordered[i].note->id))
                {
                    ordered[i-1].note->connectedToNext=false;
                    ordered[i].note->connectedToPrevious=false;
                    ordered[i-1].clip->glideConnectedToNext=false;
                    ordered[i].clip->glideConnectedFromPrevious=false;
                }
            for (auto& item : ordered) if (selected(item.note->id))
            {
                item.note->connectedToPrevious=item.note->connectedToNext=false;
                item.note->utauAutoPitchTransition=false;
                item.clip->glideConnectedFromPrevious=item.clip->glideConnectedToNext=false;
            }
        }
        for (auto& track : updated.tracks) for (auto& clip : track.clips) for (auto& note : clip.notes)
            for (const auto& link : updated.nativeConnections)
                if (selected(link.leftNoteId) || selected(link.rightNoteId))
                {
                    if (note.id==link.leftNoteId) {note.connectedToNext=false;clip.glideConnectedToNext=false;}
                    if (note.id==link.rightNoteId) {note.connectedToPrevious=false;clip.glideConnectedFromPrevious=false;}
                }
        std::erase_if(updated.nativeConnections,[&](const auto& link)
            {return selected(link.leftNoteId)||selected(link.rightNoteId);});
        for (auto& track : updated.tracks)
        {
            if (!trackShowsAllNativeRegions(track)) continue;
            std::vector<ClipData> clips;
            for (const auto& clip : track.clips)
            {
                if (std::none_of(clip.notes.begin(),clip.notes.end(),[&](const auto& n){return selected(n.id);}))
                {clips.push_back(clip);continue;}
                auto parts=disconnectedNativeClip(clip,ids);if (!parts) return false;
                for (std::size_t i=0;i<parts->size();++i)
                {
                    auto part=std::move((*parts)[i]);part.id=i==0?clip.id:makeId("clip");
                    for (auto& note : part.notes) note.clipPartId.clear();
                    clips.push_back(std::move(part));
                }
            }
            track.clips=std::move(clips);
        }
        pushUndoLocked();project=std::move(updated);
    }
    sendChangeMessage();return true;
}

void ProjectModel::setNotesConnection(const std::vector<juce::String>& noteIds,
                                       bool enabled)
{
    if (noteIds.size() < 2) return;
    const auto includes = [&](const juce::String& id)
    {
        return std::find(noteIds.begin(), noteIds.end(), id) != noteIds.end();
    };
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        struct Positioned
        {
            NoteData* note = nullptr;
            ClipData* clip = nullptr;
            double start = 0.0;
        };
        std::vector<std::vector<Positioned>> groups;
        for (auto& track : project.tracks)
        {
            std::vector<Positioned> selected;
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    if (includes(note.id))
                        selected.push_back({ &note, &clip,
                            clip.startSeconds + note.startSeconds });
            if (selected.size() >= 2)
            {
                std::stable_sort(selected.begin(), selected.end(),
                    [](const auto& left, const auto& right) { return left.start < right.start; });
                groups.push_back(std::move(selected));
            }
        }
        if (groups.empty()) return;
        pushUndoLocked();
        // A note has one incoming and one outgoing native boundary. Clear both
        // sides of replaced records before erasing them, so a forced
        // connection cannot silently leave stale compatibility booleans.
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                    for (const auto& connection : project.nativeConnections)
                        if (includes(connection.leftNoteId) || includes(connection.rightNoteId))
                        {
                            if (note.id == connection.leftNoteId) note.connectedToNext = false;
                            if (note.id == connection.rightNoteId) note.connectedToPrevious = false;
                        }
        std::erase_if(project.nativeConnections, [&](const auto& connection)
        {
            return includes(connection.leftNoteId) || includes(connection.rightNoteId);
        });
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                for (auto& note : clip.notes)
                {
                    if (includes(note.id))
                    {
                        note.connectedToPrevious = false;
                        note.connectedToNext = false;
                    }
                    if (enabled)
                        for (const auto& group : groups)
                            for (std::size_t index = 1; index < group.size(); ++index)
                                if (group[index - 1].note->id == note.id)
                                    note.connectedToNext = true;
                }
        if (enabled)
            for (const auto& group : groups)
                for (std::size_t index = 1; index < group.size(); ++index)
                {
                    auto* left = group[index - 1].note;
                    auto* right = group[index].note;
                    if (group[index - 1].clip != group[index].clip)
                    {
                        group[index - 1].clip->glideConnectedToNext = true;
                        group[index].clip->glideConnectedFromPrevious = true;
                    }
                    right->connectedToPrevious = true;
                    left->connectedToNext = true;
                    project.nativeConnections.push_back({
                        makeId("connection"), left->id, right->id,
                        "pitch-and-amplitude",
                        (group[index - 1].start + group[index].start) * 0.5,
                        {}, {} });
                }
        changed = true;
    }
    if (changed) sendChangeMessage();
}

void ProjectModel::applySourceSettings(const juce::File& source,
                                       const std::vector<SampleRegionSetting>& rows)
{
    if (rows.empty()) return;
    auto changed = false;
    {
        const juce::ScopedLock guard(lock);
        struct Entry { ClipData* clip; NoteData* note; };
        std::vector<Entry> entries;
        for (auto& track : project.tracks)
            for (auto& clip : track.clips)
                if (clip.sourceFile == source)
                    for (auto& note : clip.notes) entries.push_back({ &clip, &note });
        std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right)
        {
            return left.clip->sourceOffsetSeconds < right.clip->sourceOffsetSeconds;
        });
        if (entries.empty()) return;
        pushUndoLocked();
        for (std::size_t index = 0; index < std::min(entries.size(), rows.size()); ++index)
        {
            auto& clip = *entries[index].clip;
            auto& note = *entries[index].note;
            const auto& row = rows[index];
            note.label = row.name.trim().isEmpty() ? "-" : row.name.trim();
            note.nativeRole = row.role;
            note.nativeProvenance = row.provenance;
            note.nativeConfidence = row.confidence;
            note.nativeSourceStartSeconds = row.regionStartSeconds;
            note.nativeSourceEndSeconds = row.regionEndSeconds;
            note.nativeSegments = SampleSettings::nativeSegmentsFor(row);
            clip.sourceOffsetSeconds = std::max(0.0, row.regionStartSeconds);
            clip.sourceDurationSeconds = std::max(0.001,
                row.regionEndSeconds - row.regionStartSeconds);
            // Source-region editing establishes a new linear mapping.  An old
            // MPD warp refers to the previous source range and must not be
            // silently applied to the newly selected samples.
            clip.sourceTimeMap.clear();
            note.gain = juce::jlimit(0.0f, 4.0f,
                static_cast<float>(row.melodyneAmplitude));
            const auto targetPerSource = clip.durationSeconds / clip.sourceDurationSeconds;
            note.consonantSeconds = juce::jlimit(0.0, note.durationSeconds,
                row.fixedDurationSeconds * targetPerSource);
            note.utauPreutteranceOverrideEnabled = true;
            note.utauPreutteranceSeconds = std::max(0.0,
                (row.alignmentSeconds - row.regionStartSeconds) * targetPerSource);
            note.utauOverlapOverrideEnabled = std::abs(row.overlapSeconds) > 1.0e-9;
            note.utauOverlapSeconds = row.overlapSeconds * targetPerSource;
            note.amplitudeEnvelope = row.amplitudeEnvelope;
            if (note.amplitudeEnvelope.empty() && row.melodyneAmplitude > 1.0e-6)
            {
                const auto gainDb = static_cast<float>(20.0
                    * std::log10(row.melodyneAmplitude));
                note.amplitudeEnvelope = { { 0.0, gainDb },
                    { note.durationSeconds, gainDb } };
            }
            if (note.sourceMidiCenter >= 0.0f)
                note.midiNote = juce::jlimit(0.0f, 127.0f,
                    note.sourceMidiCenter + static_cast<float>(row.relativePitchCents / 100.0));
            if (row.melodyneData)
            {
                note.drift = juce::jlimit(0.0f, 2.0f,
                    static_cast<float>(row.melodynePitchDrift));
                note.modulation = juce::jlimit(0.0f, 2.0f,
                    static_cast<float>(row.melodynePitchModulation));
                note.formantSemitones = juce::jlimit(-12.0f, 12.0f,
                    static_cast<float>(row.melodyneFormantCents / 100.0));
                note.breath = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>(row.melodyneSibilantBalance));
                note.attackSpeed = juce::jlimit(0.05f, 20.0f,
                    static_cast<float>(row.melodyneAttackSeconds > 1.0e-6
                        ? row.fixedDurationSeconds / row.melodyneAttackSeconds : 1.0));
            }
            changed = true;
        }
    }
    if (changed) sendChangeMessage();
}

juce::ValueTree ProjectModel::toValueTree(const juce::File& projectFile) const
{
    const auto data = snapshot();
    juce::ValueTree root("HachiShifterProject");
    root.setProperty("version", 18, nullptr);
    if(data.hamoodState.isNotEmpty())root.setProperty("hamoodState",data.hamoodState,nullptr);
    root.setProperty("nativeSchemaVersion", 2, nullptr);
    root.setProperty("diffSingerPitchReferenceVersion", 1, nullptr);
    root.setProperty("name", data.name, nullptr);
    root.setProperty("bpm", data.bpm, nullptr);
    root.setProperty("beatOriginSeconds", data.beatOriginSeconds, nullptr);
    root.setProperty("numerator", data.numerator, nullptr);
    root.setProperty("denominator", data.denominator, nullptr);
    root.setProperty("gridDivision", data.gridDivision, nullptr);
    root.setProperty("noteEditDivision", data.noteEditDivision, nullptr);
    root.setProperty("baseScale", data.baseScale, nullptr);

    for (const auto& change : data.tempoChanges)
    {
        juce::ValueTree tempoTree("TempoChange");
        tempoTree.setProperty("quarterPosition", change.quarterPosition, nullptr);
        tempoTree.setProperty("bpm", change.bpm, nullptr);
        root.addChild(tempoTree, -1, nullptr);
    }

    for (const auto& connection : data.nativeConnections)
    {
        juce::ValueTree connectionTree("NativeConnection");
        connectionTree.setProperty("id", connection.id, nullptr);
        connectionTree.setProperty("leftNoteId", connection.leftNoteId, nullptr);
        connectionTree.setProperty("rightNoteId", connection.rightNoteId, nullptr);
        connectionTree.setProperty("type", connection.type, nullptr);
        connectionTree.setProperty("boundarySeconds", connection.boundarySeconds, nullptr);
        for (const auto& point : connection.pitchCurve)
        {
            juce::ValueTree pointTree("PitchCurvePoint");
            pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
            pointTree.setProperty("targetMidi", point.targetMidi, nullptr);
            pointTree.setProperty("shape", pitchCurveShapeName(point.shape), nullptr);
            pointTree.setProperty("bezierX1", point.bezierX1, nullptr);
            pointTree.setProperty("bezierY1", point.bezierY1, nullptr);
            pointTree.setProperty("bezierX2", point.bezierX2, nullptr);
            pointTree.setProperty("bezierY2", point.bezierY2, nullptr);
            connectionTree.addChild(pointTree, -1, nullptr);
        }
        for (const auto& point : connection.amplitudeCurve)
        {
            juce::ValueTree pointTree("AmplitudeCurvePoint");
            pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
            pointTree.setProperty("gainDb", point.gainDb, nullptr);
            connectionTree.addChild(pointTree, -1, nullptr);
        }
        root.addChild(connectionTree, -1, nullptr);
    }

    for (const auto& track : data.tracks)
    {
        juce::ValueTree trackTree("Track");
        trackTree.setProperty("id", track.id, nullptr);
        trackTree.setProperty("name", track.name, nullptr);
        trackTree.setProperty("compose", track.compose && !track.accompaniment, nullptr);
        trackTree.setProperty("accompaniment", track.accompaniment, nullptr);
        trackTree.setProperty("muted", track.muted, nullptr);
        trackTree.setProperty("solo", track.solo, nullptr);
        trackTree.setProperty("referenceOnly", track.referenceOnly, nullptr);
        trackTree.setProperty("volume", track.volume, nullptr);
        trackTree.setProperty("pan", track.pan, nullptr);
        trackTree.setProperty("smoothOverlaps", track.smoothOverlaps, nullptr);
        trackTree.setProperty("allowNativeAudioOverlap",track.allowNativeAudioOverlap,nullptr);
        trackTree.setProperty("normalizeVolume", track.normalizeVolume, nullptr);
        trackTree.setProperty("nsfSmoothPitchTransitions", track.nsfSmoothPitchTransitions, nullptr);
        trackTree.setProperty("nsfNoiseProtection", track.nsfNoiseProtection, nullptr);
        trackTree.setProperty("nativeNsfAudio", track.nativeNsfAudio, nullptr);
        trackTree.setProperty("voicebankDirectory",
                              track.voicebankDirectory.getFullPathName(), nullptr);
        trackTree.setProperty("utauConsonantVelocity", track.utauConsonantVelocity, nullptr);
        trackTree.setProperty("diffSingerLanguage", track.diffSingerLanguage, nullptr);
        trackTree.setProperty("diffSingerSpeaker", track.diffSingerSpeaker, nullptr);
        trackTree.setProperty("diffSingerDictionary", track.diffSingerDictionary, nullptr);
        trackTree.setProperty("utauGlobalFlags", track.utauGlobalFlags, nullptr);
        trackTree.setProperty("ustSourceDocument",track.ustSourceDocument,nullptr);
        trackTree.setProperty("ustSourceBytes",track.ustSourceBytes,nullptr);
        trackTree.setProperty("ustSourceEncoding",track.ustSourceEncoding,nullptr);
        trackTree.setProperty("ustSourceBom",track.ustSourceBom,nullptr);
        trackTree.setProperty("ustBaseline",track.ustBaseline,nullptr);
        trackTree.setProperty("utauMode", utauModeKey(track.utauMode), nullptr);
        trackTree.setProperty("utauPhonemizer", track.chineseCvvc ? "zh-cvvc" : "manual", nullptr);
        trackTree.setProperty("outputEngine", utauOutputEngineKey(track.outputEngine), nullptr);
        trackTree.setProperty("outputResampler", track.outputResampler.getFullPathName(), nullptr);
        trackTree.setProperty("outputWavtool", track.outputWavtool.getFullPathName(), nullptr);
        if (projectFile != juce::File{})
            for (const auto& entry : { std::make_pair("outputResampler", track.outputResampler),
                                     std::make_pair("outputWavtool", track.outputWavtool) })
                if (entry.second != juce::File{})
                    trackTree.setProperty(juce::String(entry.first) + "Relative",
                        entry.second.getRelativePathFrom(projectFile.getParentDirectory()), nullptr);
        // Still written so a build from before 谋 opens the project in the
        // nearest mode it has rather than dropping to plain UTAU.
        trackTree.setProperty("utauFourRegion",
                              utauModeUsesRegions(track.utauMode), nullptr);
        if (projectFile != juce::File{} && track.voicebankDirectory != juce::File{})
        {
            const auto relative = track.voicebankDirectory.getRelativePathFrom(
                projectFile.getParentDirectory());
            if (relative.isNotEmpty() && !juce::File::isAbsolutePath(relative))
                trackTree.setProperty("voicebankDirectoryRelative", relative, nullptr);
        }
        trackTree.setProperty("pitchAlgorithm", pitchAlgorithmName(track.pitchAlgorithm), nullptr);
        trackTree.setProperty("stretchAlgorithm", stretchAlgorithmName(track.stretchAlgorithm), nullptr);
        trackTree.setProperty("renderOrder", renderOrderName(track.renderOrder), nullptr);

        for (const auto& clip : track.clips)
        {
            juce::ValueTree clipTree("Clip");
            clipTree.setProperty("id", clip.id, nullptr);
            clipTree.setProperty("sourceFile", clip.sourceFile.getFullPathName(), nullptr);
            if (projectFile != juce::File{} && clip.sourceFile != juce::File{})
            {
                const auto relative = clip.sourceFile.getRelativePathFrom(
                    projectFile.getParentDirectory());
                if (relative.isNotEmpty() && !juce::File::isAbsolutePath(relative))
                    clipTree.setProperty("sourceFileRelative", relative, nullptr);
            }
            clipTree.setProperty("startSeconds", clip.startSeconds, nullptr);
            clipTree.setProperty("sourceOffsetSeconds", clip.sourceOffsetSeconds, nullptr);
            clipTree.setProperty("sourceDurationSeconds", clip.sourceDurationSeconds, nullptr);
            clipTree.setProperty("durationSeconds", clip.durationSeconds, nullptr);
            clipTree.setProperty("audioStartSeconds", clip.audioStartSeconds, nullptr);
            clipTree.setProperty("audioDurationSeconds", clip.audioDurationSeconds, nullptr);
            clipTree.setProperty("fadeInSeconds", clip.fadeInSeconds, nullptr);
            clipTree.setProperty("fadeOutSeconds", clip.fadeOutSeconds, nullptr);
            clipTree.setProperty("crossfadeInSeconds", clip.crossfadeInSeconds, nullptr);
            clipTree.setProperty("crossfadeOutSeconds", clip.crossfadeOutSeconds, nullptr);
            clipTree.setProperty("gain", clip.gain, nullptr);
            writeClipGainEnvelope(clipTree,clip);
            clipTree.setProperty("muted", clip.muted, nullptr);
            clipTree.setProperty("showNoteHints", clip.showNoteHints, nullptr);
            clipTree.setProperty("showNormalDisplay", clip.showNormalDisplay, nullptr);
            clipTree.setProperty("nativeAudioLinked", clip.nativeAudioLinked, nullptr);
            writeNativeTrimReference(clipTree,clip);
            clipTree.setProperty("glideConnectedToNext", clip.glideConnectedToNext, nullptr);
            clipTree.setProperty("glideConnectedFromPrevious", clip.glideConnectedFromPrevious, nullptr);

            for (const auto& point : clip.sourceTimeMap)
            {
                juce::ValueTree pointTree("SourceTimePoint");
                pointTree.setProperty("targetSeconds", point.targetSeconds, nullptr);
                pointTree.setProperty("sourceSeconds", point.sourceSeconds, nullptr);
                clipTree.addChild(pointTree, -1, nullptr);
            }

            for (const auto& note : clip.notes)
            {
                juce::ValueTree noteTree("Note");
                noteTree.setProperty("id", note.id, nullptr);
                if(note.clipPartId.isNotEmpty())noteTree.setProperty("clipPartId",note.clipPartId,nullptr);
                noteTree.setProperty("label", note.label, nullptr);
                noteTree.setProperty("nativeRole", nativeSegmentRoleName(note.nativeRole), nullptr);
                noteTree.setProperty("nativeProvenance", note.nativeProvenance, nullptr);
                noteTree.setProperty("nativeConfidence", note.nativeConfidence, nullptr);
                noteTree.setProperty("nativeSourceStartSeconds",
                    note.nativeSourceStartSeconds, nullptr);
                noteTree.setProperty("nativeSourceEndSeconds",
                    note.nativeSourceEndSeconds, nullptr);
                noteTree.setProperty("utauFlags", note.utauFlags, nullptr);
                noteTree.setProperty("diffSingerTiming", note.diffSingerTiming, nullptr);
                noteTree.setProperty("diffSingerPronunciation", note.diffSingerPronunciation, nullptr);
                noteTree.setProperty("vibratoEnabled", note.vibratoEnabled, nullptr);
                noteTree.setProperty("vibratoLengthPercent", note.vibratoLengthPercent, nullptr);
                noteTree.setProperty("vibratoCycleMs", note.vibratoCycleMs, nullptr);
                noteTree.setProperty("vibratoDepthCents", note.vibratoDepthCents, nullptr);
                noteTree.setProperty("vibratoFadeInPercent", note.vibratoFadeInPercent, nullptr);
                noteTree.setProperty("vibratoFadeOutPercent", note.vibratoFadeOutPercent, nullptr);
                noteTree.setProperty("vibratoPhasePercent", note.vibratoPhasePercent, nullptr);
                noteTree.setProperty("vibratoOffsetPercent", note.vibratoOffsetPercent, nullptr);
                noteTree.setProperty("vibratoEndPercent", note.vibratoEndPercent, nullptr);
                noteTree.setProperty("vibratoReferenceDurationSeconds", note.vibratoReferenceDurationSeconds, nullptr);
                noteTree.setProperty("vibratoTimeOffsetSeconds", note.vibratoTimeOffsetSeconds, nullptr);
                noteTree.setProperty("vibratoRealLine", note.vibratoRealLine, nullptr);
                noteTree.setProperty("utauTailFadeMode",note.utauTailFadeMode,nullptr);
                noteTree.setProperty("utauTailFadeStartFraction",note.utauTailFade.startFraction,nullptr);
                noteTree.setProperty("utauTailFadeEndFraction",note.utauTailFade.endFraction,nullptr);
                noteTree.setProperty("utauTailFadeStartGain",note.utauTailFade.startGain,nullptr);
                noteTree.setProperty("utauTailFadeEndGain",note.utauTailFade.endGain,nullptr);
                noteTree.setProperty("utauTailFadeCurvePower",note.utauTailFade.curvePower,nullptr);
                noteTree.setProperty("utauHeadEnvelopeMode",note.utauTailFade.head.mode,nullptr);
                noteTree.setProperty("utauHeadEnvelopeStartFraction",note.utauTailFade.head.startFraction,nullptr);
                noteTree.setProperty("utauHeadEnvelopeEndFraction",note.utauTailFade.head.endFraction,nullptr);
                noteTree.setProperty("utauHeadEnvelopeStartGain",note.utauTailFade.head.startGain,nullptr);
                noteTree.setProperty("utauHeadEnvelopeEndGain",note.utauTailFade.head.endGain,nullptr);
                noteTree.setProperty("utauHeadEnvelopeCurvePower",note.utauTailFade.head.curvePower,nullptr);
                noteTree.setProperty("utauHeadEnvelopeCustomCurve",note.utauTailFade.head.customCurve,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl1Time",note.utauTailFade.head.control1Time,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl1Progress",note.utauTailFade.head.control1Progress,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl2Time",note.utauTailFade.head.control2Time,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl2Progress",note.utauTailFade.head.control2Progress,nullptr);
                noteTree.setProperty("utauTailFadeCustomCurve",note.utauTailFade.customCurve,nullptr);
                noteTree.setProperty("utauTailFadeControl1Time",note.utauTailFade.control1Time,nullptr);
                noteTree.setProperty("utauTailFadeControl1Progress",note.utauTailFade.control1Progress,nullptr);
                noteTree.setProperty("utauTailFadeControl2Time",note.utauTailFade.control2Time,nullptr);
                noteTree.setProperty("utauTailFadeControl2Progress",note.utauTailFade.control2Progress,nullptr);
                noteTree.setProperty("utauTailMixed",backend::mixedEnvelopeText(note.utauTailFade),nullptr);
                noteTree.setProperty("utauHeadMixed",backend::mixedEnvelopeText(note.utauTailFade.head),nullptr);
                noteTree.setProperty("nativeEnvelope",juce::JSON::toString(backend::nativeEnvelopeToVar(note.nativeEnvelope),true,17),nullptr);
                noteTree.setProperty("amplitudeEnvelopeBasePercent",
                                     note.amplitudeEnvelopeBasePercent, nullptr);
                noteTree.setProperty("utauFlagSplit", note.utauFlagSplit, nullptr);
                noteTree.setProperty("utauFlagCurveEnabled",
                                     note.utauFlagCurveEnabled, nullptr);
                if (trackIsDiffSinger(track))
                    noteTree.setProperty("diffSingerOffsetEnabled", note.utauFlagCurveEnabled, nullptr);
                noteTree.setProperty("utauSplice", note.utauSplice, nullptr);
                noteTree.setProperty("utauAutoPitchTransition",
                                     note.utauAutoPitchTransition, nullptr);
                noteTree.setProperty("nativeIndependentPitch", note.nativeIndependentPitch, nullptr);
                noteTree.setProperty("nativeUnpitched", note.nativeUnpitched, nullptr);
                noteTree.setProperty("nativePitchHandlesPlaced", note.nativePitchHandlesPlaced, nullptr);
                noteTree.setProperty("utauRegionFlags1", note.utauRegionFlags1, nullptr);
                noteTree.setProperty("utauRegionFlags2", note.utauRegionFlags2, nullptr);
                noteTree.setProperty("utauRegionFlags3", note.utauRegionFlags3, nullptr);
                noteTree.setProperty("utauRegionFlags4", note.utauRegionFlags4, nullptr);
                noteTree.setProperty("utauJieSplitSet", note.utauJieSplitSet, nullptr);
                noteTree.setProperty("utauJieSplit1", note.utauJieSplit1, nullptr);
                noteTree.setProperty("utauJieSplit2", note.utauJieSplit2, nullptr);
                noteTree.setProperty("utauJieSplit3", note.utauJieSplit3, nullptr);
                noteTree.setProperty("utauConsonantVelocity",
                                     note.utauConsonantVelocity, nullptr);
                noteTree.setProperty("utauConsonantVelocityInherited",
                    note.utauConsonantVelocity == inheritedUtauConsonantVelocity,
                    nullptr);
                noteTree.setProperty("utauPreutteranceOverrideEnabled",
                    note.utauPreutteranceOverrideEnabled, nullptr);
                noteTree.setProperty("utauPreutteranceSeconds",
                    note.utauPreutteranceSeconds, nullptr);
                noteTree.setProperty("utauOverlapOverrideEnabled",
                    note.utauOverlapOverrideEnabled, nullptr);
                noteTree.setProperty("utauOverlapSeconds",
                    note.utauOverlapSeconds, nullptr);
                noteTree.setProperty("utauStpSeconds", note.utauStpSeconds, nullptr);
                noteTree.setProperty("utauModulationPercent",note.utauModulationPercent,nullptr);
                noteTree.setProperty("ustSourceSection",note.ustSourceSection,nullptr);
                noteTree.setProperty("ustSourceSectionIndex",note.ustSourceSectionIndex,nullptr);
                noteTree.setProperty("ustBaseline",note.ustBaseline,nullptr);
                if (note.utauOto.enabled)
                {
                    noteTree.setProperty("utauOto", true, nullptr);
                    noteTree.setProperty("utauOtoOffsetMs", note.utauOto.offsetMs, nullptr);
                    noteTree.setProperty("utauOtoConsonantMs", note.utauOto.consonantMs, nullptr);
                    noteTree.setProperty("utauOtoCutoffMs", note.utauOto.cutoffMs, nullptr);
                    noteTree.setProperty("utauOtoPreutteranceMs", note.utauOto.preutteranceMs, nullptr);
                    noteTree.setProperty("utauOtoOverlapMs", note.utauOto.overlapMs, nullptr);
                    noteTree.setProperty("utauOtoOnsetMs", note.utauOto.onsetMs, nullptr);
                    noteTree.setProperty("utauOtoGlideMs", note.utauOto.glideMs, nullptr);
                    noteTree.setProperty("utauOtoNucleusMs", note.utauOto.nucleusMs, nullptr);
                    noteTree.setProperty("utauOtoHasRegions", note.utauOto.hasRegions, nullptr);
                    noteTree.setProperty("utauOtoClasses", note.utauOto.classes, nullptr);
                }
                noteTree.setProperty("startSeconds", note.startSeconds, nullptr);
                noteTree.setProperty("durationSeconds", note.durationSeconds, nullptr);
                noteTree.setProperty("consonantSeconds", note.consonantSeconds, nullptr);
                noteTree.setProperty("melodyneConsonantCandidate",
                    note.melodyneConsonantCandidate, nullptr);
                noteTree.setProperty("melodyneVowelNoteId",
                    note.melodyneVowelNoteId, nullptr);
                noteTree.setProperty("midiNote", note.midiNote, nullptr);
                noteTree.setProperty("sourceMidiCenter", note.sourceMidiCenter, nullptr);
                noteTree.setProperty("sourcePitchMeasured", note.sourcePitchMeasured, nullptr);
                noteTree.setProperty("modulation", note.modulation, nullptr);
                noteTree.setProperty("drift", note.drift, nullptr);
                noteTree.setProperty("tension", note.tension, nullptr);
                noteTree.setProperty("breath", note.breath, nullptr);
                noteTree.setProperty("formantSemitones", note.formantSemitones, nullptr);
                noteTree.setProperty("gain", note.gain, nullptr);
                noteTree.setProperty("attackSpeed", note.attackSpeed, nullptr);
                noteTree.setProperty("utauTailFadeMode",note.utauTailFadeMode,nullptr);
                noteTree.setProperty("utauTailFadeStartFraction",note.utauTailFade.startFraction,nullptr);
                noteTree.setProperty("utauTailFadeEndFraction",note.utauTailFade.endFraction,nullptr);
                noteTree.setProperty("utauTailFadeStartGain",note.utauTailFade.startGain,nullptr);
                noteTree.setProperty("utauTailFadeEndGain",note.utauTailFade.endGain,nullptr);
                noteTree.setProperty("utauTailFadeCurvePower",note.utauTailFade.curvePower,nullptr);
                noteTree.setProperty("utauHeadEnvelopeMode",note.utauTailFade.head.mode,nullptr);
                noteTree.setProperty("utauHeadEnvelopeStartFraction",note.utauTailFade.head.startFraction,nullptr);
                noteTree.setProperty("utauHeadEnvelopeEndFraction",note.utauTailFade.head.endFraction,nullptr);
                noteTree.setProperty("utauHeadEnvelopeStartGain",note.utauTailFade.head.startGain,nullptr);
                noteTree.setProperty("utauHeadEnvelopeEndGain",note.utauTailFade.head.endGain,nullptr);
                noteTree.setProperty("utauHeadEnvelopeCurvePower",note.utauTailFade.head.curvePower,nullptr);
                noteTree.setProperty("utauHeadEnvelopeCustomCurve",note.utauTailFade.head.customCurve,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl1Time",note.utauTailFade.head.control1Time,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl1Progress",note.utauTailFade.head.control1Progress,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl2Time",note.utauTailFade.head.control2Time,nullptr);
                noteTree.setProperty("utauHeadEnvelopeControl2Progress",note.utauTailFade.head.control2Progress,nullptr);
                noteTree.setProperty("utauTailFadeCustomCurve",note.utauTailFade.customCurve,nullptr);
                noteTree.setProperty("utauTailFadeControl1Time",note.utauTailFade.control1Time,nullptr);
                noteTree.setProperty("utauTailFadeControl1Progress",note.utauTailFade.control1Progress,nullptr);
                noteTree.setProperty("utauTailFadeControl2Time",note.utauTailFade.control2Time,nullptr);
                noteTree.setProperty("utauTailFadeControl2Progress",note.utauTailFade.control2Progress,nullptr);
                noteTree.setProperty("utauTailMixed",backend::mixedEnvelopeText(note.utauTailFade),nullptr);
                noteTree.setProperty("utauHeadMixed",backend::mixedEnvelopeText(note.utauTailFade.head),nullptr);
                noteTree.setProperty("nativeEnvelope",juce::JSON::toString(backend::nativeEnvelopeToVar(note.nativeEnvelope),true,17),nullptr);
                noteTree.setProperty("amplitudeEnvelopeBase",
                                     note.amplitudeEnvelopeBasePercent, nullptr);
                noteTree.setProperty("robustPitchCurve", note.robustPitchCurve, nullptr);
                noteTree.setProperty("connectedToPrevious", note.connectedToPrevious, nullptr);
                noteTree.setProperty("connectedToNext", note.connectedToNext, nullptr);
                for (const auto& point : note.contour)
                {
                    juce::ValueTree pointTree("PitchPoint");
                    pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
                    pointTree.setProperty("relativeCents", point.relativeCents, nullptr);
                    pointTree.setProperty("withoutVibratoCents", point.withoutVibratoCents, nullptr);
                    pointTree.setProperty("voiced", point.voiced, nullptr);
                    pointTree.setProperty("manualTargetCents", point.manualTargetCents, nullptr);
                    pointTree.setProperty("hasManualTarget", point.hasManualTarget, nullptr);
                    noteTree.addChild(pointTree, -1, nullptr);
                }
                for (const auto& point : note.pitchControlPoints)
                {
                    juce::ValueTree pointTree("PitchControlPoint");
                    pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
                    pointTree.setProperty("targetMidi", point.targetMidi, nullptr);
                    pointTree.setProperty("shape", pitchCurveShapeName(point.shape), nullptr);
                    pointTree.setProperty("bezierX1", point.bezierX1, nullptr);
                    pointTree.setProperty("bezierY1", point.bezierY1, nullptr);
                    pointTree.setProperty("bezierX2", point.bezierX2, nullptr);
                    pointTree.setProperty("bezierY2", point.bezierY2, nullptr);
                    if (point.diffSingerRestoreSupport)
                        pointTree.setProperty("diffSingerRestoreSupport", true, nullptr);
                    noteTree.addChild(pointTree, -1, nullptr);
                }
                if (!note.diffSingerPitchReference.empty() && note.diffSingerPitchReferenceFromSavedPitch)
                    noteTree.setProperty("diffSingerPitchReferenceFromSavedPitch", true, nullptr);
                for (const auto& point : note.diffSingerPitchOffset)
                {
                    juce::ValueTree pointTree("DiffSingerPitchOffsetPoint");
                    pointTree.setProperty("timeSeconds",point.timeSeconds,nullptr);
                    pointTree.setProperty("semitones",point.targetMidi,nullptr);
                    noteTree.addChild(pointTree,-1,nullptr);
                }
                for (const auto& point : note.diffSingerPitchReference)
                {
                    juce::ValueTree pointTree("DiffSingerPitchReferencePoint");
                    pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
                    pointTree.setProperty("targetMidi", point.targetMidi, nullptr);
                    pointTree.setProperty("shape", pitchCurveShapeName(point.shape), nullptr);
                    pointTree.setProperty("bezierX1", point.bezierX1, nullptr);
                    pointTree.setProperty("bezierY1", point.bezierY1, nullptr);
                    pointTree.setProperty("bezierX2", point.bezierX2, nullptr);
                    pointTree.setProperty("bezierY2", point.bezierY2, nullptr);
                    noteTree.addChild(pointTree, -1, nullptr);
                }
                for (const auto& point : note.amplitudeEnvelope)
                {
                    juce::ValueTree pointTree("AmplitudeEnvelopePoint");
                    pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
                    pointTree.setProperty("gainDb", point.gainDb, nullptr);
                    if (point.linearToNext)
                        pointTree.setProperty("linearToNext", true, nullptr);
                    if (point.nativeSeamAnchor) pointTree.setProperty("nativeSeamAnchor", true, nullptr);
                    noteTree.addChild(pointTree, -1, nullptr);
                }
                for (const auto& segment : note.nativeSegments)
                {
                    juce::ValueTree segmentTree("NativeSegment");
                    segmentTree.setProperty("id", segment.id, nullptr);
                    segmentTree.setProperty("alias", segment.alias, nullptr);
                    segmentTree.setProperty("role", nativeSegmentRoleName(segment.role), nullptr);
                    segmentTree.setProperty("sourceStartSeconds", segment.sourceStartSeconds, nullptr);
                    segmentTree.setProperty("sourceEndSeconds", segment.sourceEndSeconds, nullptr);
                    segmentTree.setProperty("provenance", segment.provenance, nullptr);
                    segmentTree.setProperty("confidence", segment.confidence, nullptr);
                    segmentTree.setProperty("alignmentSeconds", segment.alignmentSeconds, nullptr);
                    segmentTree.setProperty("overlapSeconds", segment.overlapSeconds, nullptr);
                    segmentTree.setProperty("stretchable", segment.stretchable, nullptr);
                    segmentTree.setProperty("stretchWeight", segment.stretchWeight, nullptr);
                    noteTree.addChild(segmentTree, -1, nullptr);
                }
                for (const auto& curve : note.utauFlagCurves)
                {
                    if (curve.points.empty() && curve.flag.startsWith("HIFI:")) {
                        juce::ValueTree overrideTree("FlagCurveOverride");
                        overrideTree.setProperty("flag",curve.flag,nullptr);
                        noteTree.addChild(overrideTree,-1,nullptr);
                    }
                for (const auto& point : curve.points)
                {
                    juce::ValueTree pointTree("FlagCurvePoint");
                    pointTree.setProperty("flag", curve.flag, nullptr);
                    pointTree.setProperty("timeSeconds", point.timeSeconds, nullptr);
                    pointTree.setProperty("value", point.value, nullptr);
                    pointTree.setProperty("shape", pitchCurveShapeName(point.shape),
                                          nullptr);
                    pointTree.setProperty("bezierX1", point.bezierX1, nullptr);
                    pointTree.setProperty("bezierY1", point.bezierY1, nullptr);
                    pointTree.setProperty("bezierX2", point.bezierX2, nullptr);
                    pointTree.setProperty("bezierY2", point.bezierY2, nullptr);
                    noteTree.addChild(pointTree, -1, nullptr);
                }
                }
                for (const auto marker : note.sibilantMarkers)
                {
                    juce::ValueTree markerTree("Sibilant");
                    markerTree.setProperty("timeSeconds", marker, nullptr);
                    noteTree.addChild(markerTree, -1, nullptr);
                }
                clipTree.addChild(noteTree, -1, nullptr);
            }
            if(!clip.parts.empty())
            {
                ProjectModel partsModel;
                partsModel.project.tracks.clear();
                TrackData sources;sources.id="sources";sources.clips=clip.parts;
                partsModel.project.tracks.push_back(std::move(sources));
                juce::ValueTree partsTree("ClipParts");
                partsTree.addChild(partsModel.toValueTree(projectFile),-1,nullptr);
                clipTree.addChild(partsTree,-1,nullptr);
            }
            trackTree.addChild(clipTree, -1, nullptr);
        }
        root.addChild(trackTree, -1, nullptr);
    }
    return root;
}

ProjectData ProjectModel::fromValueTree(const juce::ValueTree& root,
                                        const juce::File& projectFile)
{
    ProjectData data;
    data.hamoodState=root["hamoodState"].toString();
    const auto projectDirectory = projectFile.getParentDirectory();
    std::map<juce::String, juce::File> recursiveMedia;
    auto indexedMedia = false;
    const auto resolveSource = [&](const juce::ValueTree& clipTree)
    {
        const auto storedPath = clipTree.getProperty("sourceFile").toString();
        juce::File source(storedPath);
        if (source.existsAsFile()) return source;
        const auto relative = clipTree.getProperty("sourceFileRelative").toString();
        if (relative.isNotEmpty())
        {
            const auto candidate = projectDirectory.getChildFile(relative);
            if (candidate.existsAsFile()) return candidate;
        }
        const auto fileName = source.getFileName();
        if (fileName.isNotEmpty())
        {
            const auto besideProject = projectDirectory.getChildFile(fileName);
            if (besideProject.existsAsFile()) return besideProject;
            if (!indexedMedia && projectDirectory.isDirectory())
            {
                indexedMedia = true;
                juce::Array<juce::File> files;
                projectDirectory.findChildFiles(files, juce::File::findFiles, true);
                for (const auto& file : files)
                    recursiveMedia.try_emplace(file.getFileName().toLowerCase(), file);
            }
            if (const auto found = recursiveMedia.find(fileName.toLowerCase());
                found != recursiveMedia.end()) return found->second;
        }
        return source;
    };
    data.name = root.getProperty("name", "Untitled").toString();
    data.bpm = static_cast<double>(root.getProperty("bpm", 120.0));
    data.beatOriginSeconds = static_cast<double>(root.getProperty("beatOriginSeconds", 0.0));
    data.numerator = static_cast<int>(root.getProperty("numerator", 4));
    data.denominator = static_cast<int>(root.getProperty("denominator", 4));
    data.gridDivision = root.getProperty("gridDivision", "1/16").toString();
    data.noteEditDivision = juce::jlimit(2, 128,
        static_cast<int>(root.getProperty("noteEditDivision", 64)));
    data.baseScale = root.getProperty("baseScale", "C").toString();

    for (const auto child : root)
        if (child.hasType("TempoChange"))
        {
            const auto position = std::max(0.0,
                static_cast<double>(child.getProperty("quarterPosition", 0.0)));
            const auto tempo = juce::jlimit(20.0, 400.0,
                static_cast<double>(child.getProperty("bpm", data.bpm)));
            if (position > 1.0e-7) data.tempoChanges.push_back({ position, tempo });
        }
    std::stable_sort(data.tempoChanges.begin(), data.tempoChanges.end(),
        [](const auto& left, const auto& right)
        {
            return left.quarterPosition < right.quarterPosition;
        });

    for (const auto connectionTree : root)
        if (connectionTree.hasType("NativeConnection"))
        {
            NativeConnection connection;
            connection.id = connectionTree.getProperty("id").toString();
            connection.leftNoteId = connectionTree.getProperty("leftNoteId").toString();
            connection.rightNoteId = connectionTree.getProperty("rightNoteId").toString();
            connection.type = connectionTree.getProperty("type", "pitch-and-amplitude").toString();
            connection.boundarySeconds = static_cast<double>(
                connectionTree.getProperty("boundarySeconds", 0.0));
            for (const auto pointTree : connectionTree)
            {
                if (pointTree.hasType("PitchCurvePoint"))
                    connection.pitchCurve.push_back({
                        static_cast<double>(pointTree.getProperty("timeSeconds", 0.0)),
                        static_cast<float>(pointTree.getProperty("targetMidi", 60.0)),
                        parsePitchCurveShape(pointTree.getProperty("shape", "natural").toString()),
                        static_cast<float>(pointTree.getProperty("bezierX1", 0.33)),
                        static_cast<float>(pointTree.getProperty("bezierY1", 0.0)),
                        static_cast<float>(pointTree.getProperty("bezierX2", 0.67)),
                        static_cast<float>(pointTree.getProperty("bezierY2", 1.0)) });
                else if (pointTree.hasType("AmplitudeCurvePoint"))
                    connection.amplitudeCurve.push_back({
                        static_cast<double>(pointTree.getProperty("timeSeconds", 0.0)),
                        static_cast<float>(pointTree.getProperty("gainDb", 0.0)) });
            }
            data.nativeConnections.push_back(std::move(connection));
        }

    for (const auto trackTree : root)
    {
        if (!trackTree.hasType("Track")) continue;
        TrackData track;
        track.id = trackTree.getProperty("id").toString();
        track.name = trackTree.getProperty("name").toString();
        track.accompaniment = static_cast<bool>(trackTree.getProperty("accompaniment", false));
        track.compose = !track.accompaniment && static_cast<bool>(trackTree.getProperty("compose", true));
        track.muted = static_cast<bool>(trackTree.getProperty("muted", false));
        track.solo = static_cast<bool>(trackTree.getProperty("solo", false));
        track.referenceOnly =
            !track.accompaniment && static_cast<bool>(trackTree.getProperty("referenceOnly", false));
        track.volume = static_cast<float>(trackTree.getProperty("volume", 1.0));
        std::vector<GainEnvelopePoint> legacyTrackEnvelope;
        for (const auto pointTree : trackTree)
            if (pointTree.hasType("TrackGainPoint"))
                legacyTrackEnvelope.push_back({static_cast<double>(pointTree.getProperty("timeSeconds", 0.0)),
                    static_cast<float>(pointTree.getProperty("gainDb", 0.0))});
        legacyTrackEnvelope = normaliseTrackGainEnvelope(std::move(legacyTrackEnvelope));
        track.pan = static_cast<float>(trackTree.getProperty("pan", 0.0));
        track.smoothOverlaps = static_cast<bool>(trackTree.getProperty("smoothOverlaps", false));
        track.allowNativeAudioOverlap=static_cast<bool>(trackTree.getProperty("allowNativeAudioOverlap",false));
        track.normalizeVolume = static_cast<bool>(trackTree.getProperty("normalizeVolume", false));
        track.nsfSmoothPitchTransitions = static_cast<bool>(trackTree.getProperty("nsfSmoothPitchTransitions", true));
        track.nsfNoiseProtection = static_cast<bool>(trackTree.getProperty("nsfNoiseProtection", true));
        track.nativeNsfAudio = static_cast<bool>(trackTree.getProperty("nativeNsfAudio", false));
        track.voicebankDirectory = juce::File(
            trackTree.getProperty("voicebankDirectory").toString());
        track.utauConsonantVelocity = static_cast<int>(
            trackTree.getProperty("utauConsonantVelocity", 100));
        track.utauGlobalFlags = trackTree.getProperty("utauGlobalFlags").toString();
        track.ustSourceDocument=trackTree.getProperty("ustSourceDocument").toString();
        track.ustSourceBytes=trackTree.getProperty("ustSourceBytes").toString();
        track.ustSourceEncoding=trackTree.getProperty("ustSourceEncoding").toString();
        track.ustSourceBom=(bool)trackTree.getProperty("ustSourceBom",false);
        track.ustBaseline=trackTree.getProperty("ustBaseline").toString();
        track.diffSingerLanguage = trackTree.getProperty("diffSingerLanguage", "zh").toString();
        track.diffSingerSpeaker = trackTree.getProperty("diffSingerSpeaker").toString();
        track.diffSingerDictionary = trackTree.getProperty("diffSingerDictionary").toString();
        track.chineseCvvc = trackTree.getProperty("utauPhonemizer", "manual").toString() == "zh-cvvc";
        // A project written before 谋 carries only the flag.
        track.utauMode = trackTree.hasProperty("utauMode")
            ? parseUtauMode(trackTree.getProperty("utauMode", "").toString())
            : (static_cast<bool>(trackTree.getProperty("utauFourRegion", false))
                   ? UtauMode::jie : UtauMode::classic);
        if (!track.voicebankDirectory.isDirectory())
        {
            const auto relative = trackTree.getProperty("voicebankDirectoryRelative").toString();
            if (relative.isNotEmpty())
            {
                const auto candidate = projectDirectory.getChildFile(relative);
                if (candidate.isDirectory()) track.voicebankDirectory = candidate;
            }
        }
        track.pitchAlgorithm = parsePitchAlgorithm(trackTree.getProperty("pitchAlgorithm", "mld5").toString());
        track.outputEngine = parseUtauOutputEngine(trackTree.getProperty("outputEngine").toString());
        const auto engineFile = [&](const char* key) {
            juce::File file(trackTree.getProperty(key).toString());
            const auto relative = trackTree.getProperty(juce::String(key) + "Relative").toString();
            if (!file.existsAsFile() && relative.isNotEmpty()) {
                const auto candidate = projectDirectory.getChildFile(relative);
                if (candidate.existsAsFile()) file = candidate;
            }
            return file;
        };
        track.outputResampler = engineFile("outputResampler");
        track.outputWavtool = engineFile("outputWavtool");
        if (!track.nativeNsfAudio && !trackTree.hasProperty("outputEngine") && track.pitchAlgorithm == PitchAlgorithm::nsfHifigan
            && !track.accompaniment && (utauModeUsesRegions(track.utauMode) || track.voicebankDirectory != juce::File{})
            && !track.voicebankDirectory.getChildFile("dsconfig.yaml").existsAsFile()) {
            track.pitchAlgorithm = PitchAlgorithm::utau;
            track.outputEngine = UtauOutputEngine::pcNsfHifigan;
        }
        if (track.voicebankDirectory.getChildFile("dsconfig.yaml").existsAsFile()) track.utauMode = UtauMode::mou;
        track.stretchAlgorithm = parseStretchAlgorithm(trackTree.getProperty("stretchAlgorithm", "melodyne-hybrid").toString());
        track.renderOrder = parseRenderOrder(trackTree.getProperty("renderOrder", "process-then-splice").toString());

        for (const auto clipTree : trackTree)
        {
            if (!clipTree.hasType("Clip")) continue;
            ClipData clip;
            clip.id = clipTree.getProperty("id").toString();
            clip.sourceFile = resolveSource(clipTree);
            clip.startSeconds = static_cast<double>(clipTree.getProperty("startSeconds", 0.0));
            clip.sourceOffsetSeconds = static_cast<double>(clipTree.getProperty("sourceOffsetSeconds", 0.0));
            clip.sourceDurationSeconds = static_cast<double>(clipTree.getProperty("sourceDurationSeconds", 0.0));
            clip.durationSeconds = static_cast<double>(clipTree.getProperty("durationSeconds", 1.0));
            clip.audioStartSeconds = juce::jlimit(0.0, std::max(0.0, clip.durationSeconds),
                static_cast<double>(clipTree.getProperty("audioStartSeconds", 0.0)));
            clip.audioDurationSeconds = static_cast<double>(clipTree.getProperty("audioDurationSeconds", -1.0));
            if (clip.audioDurationSeconds >= 0.0)
                clip.audioDurationSeconds = juce::jlimit(0.0, std::max(0.0, clip.durationSeconds - clip.audioStartSeconds), clip.audioDurationSeconds);
            clip.fadeInSeconds = static_cast<double>(clipTree.getProperty("fadeInSeconds", 0.0));
            clip.fadeOutSeconds = static_cast<double>(clipTree.getProperty("fadeOutSeconds", 0.0));
            clip.crossfadeInSeconds = static_cast<double>(clipTree.getProperty("crossfadeInSeconds", 0.0));
            clip.crossfadeOutSeconds = static_cast<double>(clipTree.getProperty("crossfadeOutSeconds", 0.0));
            clip.gain = static_cast<float>(clipTree.getProperty("gain", 1.0));
            readClipGainEnvelope(clipTree,clip);
            if(!legacyTrackEnvelope.empty())
            {
                auto local=legacyTrackEnvelope;shiftGainEnvelope(local,-clip.startSeconds);
                if(clip.gainEnvelope.empty()){clip.gainEnvelope=std::move(local);anchorClipGainEnvelope(clip);}
                else clip.inheritedGainEnvelopes.push_back(std::move(local));
            }
            clip.muted = static_cast<bool>(clipTree.getProperty("muted", false));
            clip.showNoteHints = static_cast<bool>(clipTree.getProperty("showNoteHints", false));
            clip.showNormalDisplay = static_cast<bool>(clipTree.getProperty("showNormalDisplay", false));
            clip.nativeAudioLinked = static_cast<bool>(clipTree.getProperty("nativeAudioLinked", false));
            readNativeTrimReference(clipTree,clip);
            clip.glideConnectedToNext = static_cast<bool>(
                clipTree.getProperty("glideConnectedToNext", false));
            clip.glideConnectedFromPrevious = static_cast<bool>(
                clipTree.getProperty("glideConnectedFromPrevious", false));

            for (const auto noteTree : clipTree)
            {
                if(noteTree.hasType("ClipParts"))
                {
                    auto sources=fromValueTree(noteTree.getChild(0),projectFile);
                    if(!sources.tracks.empty())clip.parts=std::move(sources.tracks.front().clips);
                    continue;
                }
                if (noteTree.hasType("SourceTimePoint"))
                {
                    clip.sourceTimeMap.push_back({
                        static_cast<double>(noteTree.getProperty("targetSeconds", 0.0)),
                        static_cast<double>(noteTree.getProperty("sourceSeconds", 0.0)) });
                    continue;
                }
                if (!noteTree.hasType("Note")) continue;
                NoteData note;
                note.id = noteTree.getProperty("id").toString();
                note.clipPartId = noteTree.getProperty("clipPartId").toString();
                note.label = noteTree.getProperty("label").toString();
                note.nativeRole = parseNativeSegmentRole(
                    noteTree.getProperty("nativeRole", "unknown").toString());
                note.nativeProvenance = noteTree.getProperty("nativeProvenance", "estimated").toString();
                note.nativeConfidence = static_cast<float>(
                    noteTree.getProperty("nativeConfidence", 0.0));
                note.nativeSourceStartSeconds = static_cast<double>(
                    noteTree.getProperty("nativeSourceStartSeconds", -1.0));
                note.nativeSourceEndSeconds = static_cast<double>(
                    noteTree.getProperty("nativeSourceEndSeconds", -1.0));
                note.utauFlags = noteTree.getProperty("utauFlags").toString();
                note.diffSingerTiming = noteTree.getProperty("diffSingerTiming").toString();
                note.diffSingerPronunciation = noteTree.getProperty("diffSingerPronunciation").toString();
                note.diffSingerPitchReferenceFromSavedPitch = static_cast<bool>(
                    noteTree.getProperty("diffSingerPitchReferenceFromSavedPitch", false));
                const auto storedVelocity = static_cast<int>(
                    noteTree.getProperty("utauConsonantVelocity", -1));
                note.utauJieSplitSet = static_cast<bool>(
                    noteTree.getProperty("utauJieSplitSet", false));
                note.vibratoEnabled = static_cast<bool>(
                    noteTree.getProperty("vibratoEnabled", false));
                note.vibratoLengthPercent = noteTree.getProperty("vibratoLengthPercent", 65.0);
                note.vibratoCycleMs = noteTree.getProperty("vibratoCycleMs", 180.0);
                note.vibratoDepthCents = noteTree.getProperty("vibratoDepthCents", 35.0);
                note.vibratoFadeInPercent = noteTree.getProperty("vibratoFadeInPercent", 20.0);
                note.vibratoFadeOutPercent = noteTree.getProperty("vibratoFadeOutPercent", 20.0);
                note.vibratoPhasePercent = noteTree.getProperty("vibratoPhasePercent", 0.0);
                note.vibratoOffsetPercent = noteTree.getProperty("vibratoOffsetPercent", 0.0);
                // Older projects have none, and 100 is where UTAU stops.
                note.vibratoEndPercent = juce::jlimit(1.0, 100.0, static_cast<double>(
                    noteTree.getProperty("vibratoEndPercent", 100.0)));
                note.vibratoReferenceDurationSeconds = std::max(0.0, static_cast<double>(noteTree.getProperty("vibratoReferenceDurationSeconds", 0.0)));
                note.vibratoTimeOffsetSeconds = std::max(0.0, static_cast<double>(noteTree.getProperty("vibratoTimeOffsetSeconds", 0.0)));
                note.vibratoRealLine = static_cast<bool>(
                    noteTree.getProperty("vibratoRealLine", false));
                note.amplitudeEnvelopeBasePercent = juce::jlimit(0.0f, 200.0f,
                    static_cast<float>(static_cast<double>(
                        noteTree.getProperty("amplitudeEnvelopeBasePercent", 100.0))));
                note.utauTailFadeMode=juce::jlimit(0,2,(int)noteTree.getProperty("utauTailFadeMode",0));
                note.nativeEnvelope=backend::nativeEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("nativeEnvelope").toString()));
                note.utauTailFade.startFraction=(double)noteTree.getProperty("utauTailFadeStartFraction",0.0);
                note.utauTailFade.endFraction=(double)noteTree.getProperty("utauTailFadeEndFraction",1.0);
                note.utauTailFade.startGain=(double)noteTree.getProperty("utauTailFadeStartGain",1.0);
                note.utauTailFade.endGain=(double)noteTree.getProperty("utauTailFadeEndGain",0.0);
                note.utauTailFade.curvePower=(double)noteTree.getProperty("utauTailFadeCurvePower",1.0);
                note.utauTailFade.customCurve=(bool)noteTree.getProperty("utauTailFadeCustomCurve",false);
                note.utauTailFade.control1Time=(double)noteTree.getProperty("utauTailFadeControl1Time",1.0/3.0);
                note.utauTailFade.control1Progress=(double)noteTree.getProperty("utauTailFadeControl1Progress",0.0);
                note.utauTailFade.control2Time=(double)noteTree.getProperty("utauTailFadeControl2Time",2.0/3.0);
                note.utauTailFade.control2Progress=(double)noteTree.getProperty("utauTailFadeControl2Progress",1.0);
                note.utauTailFade.head.mode=(int)noteTree.getProperty("utauHeadEnvelopeMode",0);
                note.utauTailFade.head.startFraction=(double)noteTree.getProperty("utauHeadEnvelopeStartFraction",0.0);
                note.utauTailFade.head.endFraction=(double)noteTree.getProperty("utauHeadEnvelopeEndFraction",1.0);
                note.utauTailFade.head.startGain=(double)noteTree.getProperty("utauHeadEnvelopeStartGain",0.0);
                note.utauTailFade.head.endGain=(double)noteTree.getProperty("utauHeadEnvelopeEndGain",1.0);
                note.utauTailFade.head.curvePower=(double)noteTree.getProperty("utauHeadEnvelopeCurvePower",1.0);
                note.utauTailFade.head.customCurve=(bool)noteTree.getProperty("utauHeadEnvelopeCustomCurve",false);
                note.utauTailFade.head.control1Time=(double)noteTree.getProperty("utauHeadEnvelopeControl1Time",1.0/3.0);
                note.utauTailFade.head.control1Progress=(double)noteTree.getProperty("utauHeadEnvelopeControl1Progress",0.0);
                note.utauTailFade.head.control2Time=(double)noteTree.getProperty("utauHeadEnvelopeControl2Time",2.0/3.0);
                note.utauTailFade.head.control2Progress=(double)noteTree.getProperty("utauHeadEnvelopeControl2Progress",1.0);
                if(!backend::mixedEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("utauTailMixed").toString()),note.utauTailFade)
                    ||!backend::mixedEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("utauHeadMixed").toString()),note.utauTailFade.head)
                    ||!note.utauTailFade.valid())note.utauTailFade={};
                note.utauFlagSplit = static_cast<bool>(
                    noteTree.getProperty("utauFlagSplit", false));
                // Distinguish a deliberate disable from the old default-off flag.
                note.utauFlagCurveEnabled = trackIsDiffSinger(track)
                    ? static_cast<bool>(noteTree.getProperty("diffSingerOffsetEnabled", true))
                    : static_cast<bool>(noteTree.getProperty("utauFlagCurveEnabled", false));
                note.utauSplice = static_cast<bool>(
                    noteTree.getProperty("utauSplice", false));
                // A project saved before this was read carries no such
                // property, and its notes glided; they still do.
                note.utauAutoPitchTransition = static_cast<bool>(
                    noteTree.getProperty("utauAutoPitchTransition", true));
                note.nativeIndependentPitch = static_cast<bool>(noteTree.getProperty("nativeIndependentPitch", false));
                note.nativeUnpitched = static_cast<bool>(noteTree.getProperty("nativeUnpitched", false));
                note.nativePitchHandlesPlaced = static_cast<bool>(noteTree.getProperty("nativePitchHandlesPlaced", false));
                note.utauRegionFlags1 = noteTree.getProperty("utauRegionFlags1").toString();
                note.utauRegionFlags2 = noteTree.getProperty("utauRegionFlags2").toString();
                note.utauRegionFlags3 = noteTree.getProperty("utauRegionFlags3").toString();
                note.utauRegionFlags4 = noteTree.getProperty("utauRegionFlags4").toString();
                note.utauJieSplit1 = noteTree.getProperty("utauJieSplit1", 0.0);
                note.utauJieSplit2 = noteTree.getProperty("utauJieSplit2", 0.0);
                note.utauJieSplit3 = noteTree.getProperty("utauJieSplit3", 0.0);
                if (noteTree.hasProperty("utauConsonantVelocityInherited"))
                    note.utauConsonantVelocity = static_cast<bool>(noteTree.getProperty(
                        "utauConsonantVelocityInherited", false))
                            ? inheritedUtauConsonantVelocity : storedVelocity;
                else
                    // Before signed velocity support, every negative value was
                    // normalised to -1 and meant "use the track value".
                    note.utauConsonantVelocity = storedVelocity < 0
                        ? inheritedUtauConsonantVelocity : storedVelocity;
                note.utauPreutteranceOverrideEnabled = static_cast<bool>(
                    noteTree.getProperty("utauPreutteranceOverrideEnabled", false));
                note.utauPreutteranceSeconds = std::max(0.0, static_cast<double>(
                    noteTree.getProperty("utauPreutteranceSeconds", 0.0)));
                note.utauOverlapOverrideEnabled = static_cast<bool>(
                    noteTree.getProperty("utauOverlapOverrideEnabled", false));
                note.utauOverlapSeconds = static_cast<double>(
                    noteTree.getProperty("utauOverlapSeconds", 0.0));
                note.utauStpSeconds = static_cast<double>(
                    noteTree.getProperty("utauStpSeconds", 0.0));
                note.utauModulationPercent=(double)noteTree.getProperty("utauModulationPercent",0.0);
                note.ustSourceSection=noteTree.getProperty("ustSourceSection").toString();
                note.ustSourceSectionIndex=(int)noteTree.getProperty("ustSourceSectionIndex",-1);
                note.ustBaseline=noteTree.getProperty("ustBaseline").toString();
                note.utauOto.enabled = static_cast<bool>(
                    noteTree.getProperty("utauOto", false));
                if (note.utauOto.enabled)
                {
                    note.utauOto.offsetMs = static_cast<double>(
                        noteTree.getProperty("utauOtoOffsetMs", 0.0));
                    note.utauOto.consonantMs = static_cast<double>(
                        noteTree.getProperty("utauOtoConsonantMs", 0.0));
                    note.utauOto.cutoffMs = static_cast<double>(
                        noteTree.getProperty("utauOtoCutoffMs", 0.0));
                    note.utauOto.preutteranceMs = static_cast<double>(
                        noteTree.getProperty("utauOtoPreutteranceMs", 0.0));
                    note.utauOto.overlapMs = static_cast<double>(
                        noteTree.getProperty("utauOtoOverlapMs", 0.0));
                    note.utauOto.onsetMs = static_cast<double>(
                        noteTree.getProperty("utauOtoOnsetMs", 0.0));
                    note.utauOto.glideMs = static_cast<double>(
                        noteTree.getProperty("utauOtoGlideMs", 0.0));
                    note.utauOto.nucleusMs = static_cast<double>(
                        noteTree.getProperty("utauOtoNucleusMs", 0.0));
                    note.utauOto.hasRegions = static_cast<bool>(
                        noteTree.getProperty("utauOtoHasRegions", false));
                    note.utauOto.classes = noteTree.getProperty("utauOtoClasses").toString();
                }
                note.startSeconds = static_cast<double>(noteTree.getProperty("startSeconds", 0.0));
                note.durationSeconds = static_cast<double>(noteTree.getProperty("durationSeconds", 0.25));
                note.consonantSeconds = static_cast<double>(noteTree.getProperty("consonantSeconds", 0.04));
                note.melodyneConsonantCandidate = static_cast<bool>(
                    noteTree.getProperty("melodyneConsonantCandidate", false));
                note.melodyneVowelNoteId = noteTree.getProperty(
                    "melodyneVowelNoteId").toString();
                note.midiNote = static_cast<float>(noteTree.getProperty("midiNote", 60.0));
                note.sourceMidiCenter = static_cast<float>(noteTree.getProperty("sourceMidiCenter", -1.0));
                note.sourcePitchMeasured = static_cast<bool>(noteTree.getProperty("sourcePitchMeasured", false));
                note.modulation = static_cast<float>(noteTree.getProperty("modulation", 1.0));
                note.drift = static_cast<float>(noteTree.getProperty("drift", 1.0));
                note.tension = static_cast<float>(noteTree.getProperty("tension", 0.0));
                note.breath = static_cast<float>(noteTree.getProperty("breath", 0.0));
                note.formantSemitones = static_cast<float>(noteTree.getProperty("formantSemitones", 0.0));
                note.gain = static_cast<float>(noteTree.getProperty("gain", 1.0));
                note.attackSpeed = static_cast<float>(noteTree.getProperty("attackSpeed", 1.0));
                // Older projects have no base, and 100 is the envelope as drawn.
                note.amplitudeEnvelopeBasePercent = juce::jlimit(0.0f, 200.0f,
                    static_cast<float>(noteTree.getProperty("amplitudeEnvelopeBase", 100.0)));
                note.utauTailFadeMode=juce::jlimit(0,2,(int)noteTree.getProperty("utauTailFadeMode",0));
                note.nativeEnvelope=backend::nativeEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("nativeEnvelope").toString()));
                note.utauTailFade.startFraction=(double)noteTree.getProperty("utauTailFadeStartFraction",0.0);
                note.utauTailFade.endFraction=(double)noteTree.getProperty("utauTailFadeEndFraction",1.0);
                note.utauTailFade.startGain=(double)noteTree.getProperty("utauTailFadeStartGain",1.0);
                note.utauTailFade.endGain=(double)noteTree.getProperty("utauTailFadeEndGain",0.0);
                note.utauTailFade.curvePower=(double)noteTree.getProperty("utauTailFadeCurvePower",1.0);
                note.utauTailFade.customCurve=(bool)noteTree.getProperty("utauTailFadeCustomCurve",false);
                note.utauTailFade.control1Time=(double)noteTree.getProperty("utauTailFadeControl1Time",1.0/3.0);
                note.utauTailFade.control1Progress=(double)noteTree.getProperty("utauTailFadeControl1Progress",0.0);
                note.utauTailFade.control2Time=(double)noteTree.getProperty("utauTailFadeControl2Time",2.0/3.0);
                note.utauTailFade.control2Progress=(double)noteTree.getProperty("utauTailFadeControl2Progress",1.0);
                note.utauTailFade.head.mode=(int)noteTree.getProperty("utauHeadEnvelopeMode",0);
                note.utauTailFade.head.startFraction=(double)noteTree.getProperty("utauHeadEnvelopeStartFraction",0.0);
                note.utauTailFade.head.endFraction=(double)noteTree.getProperty("utauHeadEnvelopeEndFraction",1.0);
                note.utauTailFade.head.startGain=(double)noteTree.getProperty("utauHeadEnvelopeStartGain",0.0);
                note.utauTailFade.head.endGain=(double)noteTree.getProperty("utauHeadEnvelopeEndGain",1.0);
                note.utauTailFade.head.curvePower=(double)noteTree.getProperty("utauHeadEnvelopeCurvePower",1.0);
                note.utauTailFade.head.customCurve=(bool)noteTree.getProperty("utauHeadEnvelopeCustomCurve",false);
                note.utauTailFade.head.control1Time=(double)noteTree.getProperty("utauHeadEnvelopeControl1Time",1.0/3.0);
                note.utauTailFade.head.control1Progress=(double)noteTree.getProperty("utauHeadEnvelopeControl1Progress",0.0);
                note.utauTailFade.head.control2Time=(double)noteTree.getProperty("utauHeadEnvelopeControl2Time",2.0/3.0);
                note.utauTailFade.head.control2Progress=(double)noteTree.getProperty("utauHeadEnvelopeControl2Progress",1.0);
                if(!backend::mixedEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("utauTailMixed").toString()),note.utauTailFade)
                    ||!backend::mixedEnvelopeFromVar(juce::JSON::parse(noteTree.getProperty("utauHeadMixed").toString()),note.utauTailFade.head)
                    ||!note.utauTailFade.valid())note.utauTailFade={};
                note.robustPitchCurve = static_cast<bool>(
                    noteTree.getProperty("robustPitchCurve", false));
                note.connectedToPrevious = static_cast<bool>(noteTree.getProperty("connectedToPrevious", false));
                note.connectedToNext = static_cast<bool>(noteTree.getProperty("connectedToNext", false));
                for (const auto child : noteTree)
                {
                    if (child.hasType("PitchPoint"))
                    {
                        const auto relative = static_cast<float>(child.getProperty("relativeCents", 0.0));
                        note.contour.push_back({ static_cast<double>(child.getProperty("timeSeconds", 0.0)),
                                                 relative,
                                                 static_cast<float>(child.getProperty("withoutVibratoCents", relative)),
                                                 static_cast<bool>(child.getProperty("voiced", true)),
                                                 static_cast<float>(child.getProperty("manualTargetCents", 0.0)),
                                                 static_cast<bool>(child.getProperty("hasManualTarget", false)) });
                    }
                    else if (child.hasType("PitchControlPoint"))
                        note.pitchControlPoints.push_back({
                            static_cast<double>(child.getProperty("timeSeconds", 0.0)),
                            static_cast<float>(child.getProperty("targetMidi", note.midiNote)),
                            parsePitchCurveShape(child.getProperty("shape", "natural").toString()),
                            static_cast<float>(child.getProperty("bezierX1", 0.33)),
                            static_cast<float>(child.getProperty("bezierY1", 0.0)),
                            static_cast<float>(child.getProperty("bezierX2", 0.67)),
                            static_cast<float>(child.getProperty("bezierY2", 1.0)),
                            static_cast<bool>(child.getProperty("diffSingerRestoreSupport", false)) });
                    else if (child.hasType("DiffSingerPitchOffsetPoint"))
                    {
                        const auto time=static_cast<double>(child.getProperty("timeSeconds",0.0));
                        const auto value=static_cast<float>(child.getProperty("semitones",0.0));
                        if (std::isfinite(time) && std::isfinite(value))
                            note.diffSingerPitchOffset.push_back({juce::jlimit(0.0,note.durationSeconds,time),
                                juce::jlimit(-127.0f,127.0f,value),PitchCurveShape::linear});
                    }
                    else if (child.hasType("DiffSingerPitchReferencePoint"))
                        note.diffSingerPitchReference.push_back({
                            static_cast<double>(child.getProperty("timeSeconds", 0.0)),
                            static_cast<float>(child.getProperty("targetMidi", note.midiNote)),
                            parsePitchCurveShape(child.getProperty("shape", "natural").toString()),
                            static_cast<float>(child.getProperty("bezierX1", 0.33)),
                            static_cast<float>(child.getProperty("bezierY1", 0.0)),
                            static_cast<float>(child.getProperty("bezierX2", 0.67)),
                            static_cast<float>(child.getProperty("bezierY2", 1.0)) });
                    else if (child.hasType("AmplitudeEnvelopePoint"))
                        note.amplitudeEnvelope.push_back({
                            static_cast<double>(child.getProperty("timeSeconds", 0.0)),
                            juce::jlimit(-60.0f, 12.0f,
                                static_cast<float>(child.getProperty("gainDb", 0.0))),
                            static_cast<bool>(child.getProperty("linearToNext", false)),
                            static_cast<bool>(child.getProperty("nativeSeamAnchor", false)) });
                    else if (child.hasType("NativeSegment"))
                        note.nativeSegments.push_back({
                            child.getProperty("id", "segment").toString(),
                            child.getProperty("alias", "-").toString(),
                            parseNativeSegmentRole(child.getProperty("role", "unknown").toString()),
                            static_cast<double>(child.getProperty("sourceStartSeconds", 0.0)),
                            static_cast<double>(child.getProperty("sourceEndSeconds", 0.0)),
                            child.getProperty("provenance", "estimated").toString(),
                            static_cast<float>(child.getProperty("confidence", 0.0)),
                            static_cast<double>(child.getProperty("alignmentSeconds", 0.0)),
                            static_cast<double>(child.getProperty("overlapSeconds", 0.0)),
                            static_cast<bool>(child.getProperty("stretchable", true)),
                            static_cast<double>(child.getProperty("stretchWeight", 1.0)) });
                    // "FlagCurveG" is how the g curve was written before any
                    // other flag could have one; it reads as the g curve.
                    else if (child.hasType("FlagCurveOverride"))
                    {
                        const auto flag=child.getProperty("flag").toString();
                        if (flag.startsWith("HIFI:") && std::none_of(note.utauFlagCurves.begin(),note.utauFlagCurves.end(),
                            [&](const auto& c){return c.flag==flag;})) note.utauFlagCurves.push_back({flag,{}});
                    }
                    else if (child.hasType("FlagCurvePoint")
                             || child.hasType("FlagCurveG"))
                    {
                        const auto flag = child.hasType("FlagCurveG")
                            ? juce::String("g")
                            : child.getProperty("flag", "g").toString();
                        const auto& kind = flagCurveKindFor(flag);
                        auto found = std::find_if(note.utauFlagCurves.begin(),
                            note.utauFlagCurves.end(),
                            [&flag](const auto& curve) { return curve.flag == flag; });
                        if (found == note.utauFlagCurves.end())
                        {
                            note.utauFlagCurves.push_back({ flag, {} });
                            found = std::prev(note.utauFlagCurves.end());
                        }
                        found->points.push_back({
                            static_cast<double>(child.getProperty("timeSeconds", 0.0)),
                            juce::jlimit(kind.minimum, kind.maximum,
                                static_cast<float>(child.getProperty("value", 0.0))),
                            parsePitchCurveShape(
                                child.getProperty("shape", "linear").toString()),
                            static_cast<float>(child.getProperty("bezierX1", 0.33)),
                            static_cast<float>(child.getProperty("bezierY1", 0.0)),
                            static_cast<float>(child.getProperty("bezierX2", 0.67)),
                            static_cast<float>(child.getProperty("bezierY2", 1.0)) });
                    }
                    else if (child.hasType("Sibilant"))
                        note.sibilantMarkers.push_back(static_cast<double>(child.getProperty("timeSeconds", 0.0)));
                }
                for (auto& curve : note.utauFlagCurves)
                    std::stable_sort(curve.points.begin(), curve.points.end(),
                        [](const auto& left, const auto& right)
                        {
                            return left.timeSeconds < right.timeSeconds;
                        });
                // Updates before 088 stored measured source curves as dense
                // PitchPoints, without a provenance bit. Two-point authored
                // notes and voicebank/MIDI tracks must not acquire that bit.
                if (!noteTree.hasProperty("sourcePitchMeasured")
                    && !trackUsesVoicebankSynthesis(track) && !trackIsDiffSinger(track)
                    && note.nativeProvenance != "utau" && note.sourceMidiCenter >= 0.0f
                    && note.contour.size() > 2)
                    note.sourcePitchMeasured = true;
                std::stable_sort(note.amplitudeEnvelope.begin(),
                                 note.amplitudeEnvelope.end(),
                    [](const auto& left, const auto& right)
                    {
                        return left.timeSeconds < right.timeSeconds;
                    });
                // Before the reference field existed, DS files saved only the
                // current pitch. Keep that as an explicitly labelled load-time
                // reference; never replace a genuine saved prediction or create
                // a reference for a new MIDI-only note in a current-format file.
                std::stable_sort(note.diffSingerPitchOffset.begin(),note.diffSingerPitchOffset.end(),
                    [](const auto& a,const auto& b){return a.timeSeconds<b.timeSeconds;});
                if (!root.hasProperty("diffSingerPitchReferenceVersion")
                    && trackIsDiffSinger(track) && note.diffSingerPitchReference.empty()
                    && (!note.pitchControlPoints.empty()
                        || std::any_of(note.contour.begin(), note.contour.end(),
                            [](const auto& p) { return p.hasManualTarget; })))
                {
                    note.diffSingerPitchReference = ownPitchPoints(note);
                    note.diffSingerPitchReferenceFromSavedPitch = !note.diffSingerPitchReference.empty();
                }
                clip.notes.push_back(std::move(note));
            }
            std::stable_sort(clip.sourceTimeMap.begin(), clip.sourceTimeMap.end(),
                [](const auto& left, const auto& right)
                {
                    return left.targetSeconds < right.targetSeconds;
                });
            auto previousTarget = -1.0;
            auto previousSource = -1.0;
            std::erase_if(clip.sourceTimeMap, [&](auto& point)
            {
                point.targetSeconds = juce::jlimit(0.0, clip.durationSeconds,
                                                   point.targetSeconds);
                point.sourceSeconds = juce::jlimit(0.0,
                    clip.sourceDurationSeconds > 0.0 ? clip.sourceDurationSeconds
                                                     : clip.durationSeconds,
                    point.sourceSeconds);
                const auto invalid = !std::isfinite(point.targetSeconds)
                    || !std::isfinite(point.sourceSeconds)
                    || point.targetSeconds <= previousTarget + 1.0e-9
                    || point.sourceSeconds < previousSource - 1.0e-9;
                if (!invalid)
                {
                    previousTarget = point.targetSeconds;
                    previousSource = point.sourceSeconds;
                }
                return invalid;
            });
            if (clip.sourceTimeMap.size() < 2) clip.sourceTimeMap.clear();
            track.clips.push_back(std::move(clip));
        }
        data.tracks.push_back(std::move(track));
    }
    return data;
}

bool ProjectModel::save(const juce::File& file, juce::String& error) const
{
    juce::MemoryOutputStream bytes;
    toValueTree(file).writeToStream(bytes);
    return projectio::save(file, bytes.getMemoryBlock(), error, true);
}

bool ProjectModel::saveRecoverySnapshot(ProjectData data, const juce::File& recoveryFile,
                                         const juce::File& originalFile, juce::String& error)
{
    ProjectModel captured;
    captured.project = std::move(data);
    // Paths are relative to the recovery file; absolute paths are also retained.
    auto tree = captured.toValueTree(recoveryFile);
    tree.setProperty("recoveryOriginalFile", originalFile.getFullPathName(), nullptr);
    tree.setProperty("recoverySavedAt", juce::Time::getCurrentTime().toISO8601(true), nullptr);
    juce::MemoryOutputStream bytes;
    tree.writeToStream(bytes);
    return projectio::save(recoveryFile, bytes.getMemoryBlock(), error, false);
}

bool ProjectModel::load(const juce::File& file, juce::String& error)
{
    error.clear();
    // Read the file whole, then parse it out of memory.  ValueTree reads a
    // stream a field at a time and a FileInputStream turns every one of those
    // into its own read of the disk, which is why opening a project took
    // seconds and got worse the bigger it was: 970 KB was 2.8 s off the file
    // against 30 ms from a block the same file loaded into in under a
    // millisecond.  Writing never had this -- 26 ms for the same project --
    // so only this side needed it.
    juce::MemoryBlock bytes;
    if (file.loadFileAsData(bytes))
    {
        auto tree = projectio::validatedTree(bytes);
        if (tree.hasType("HachiShifterProject"))
        {
            auto loaded = fromValueTree(tree, file);
            juce::StringArray missing;
            for (const auto& track : loaded.tracks)
                for (const auto& clip : track.clips)
                    if (clip.sourceFile != juce::File{} && !clip.sourceFile.existsAsFile())
                        missing.addIfNotAlreadyThere(clip.sourceFile.getFullPathName());
            resetDocument(std::move(loaded));
            if (!missing.isEmpty())
                error = missing.joinIntoString("\n");
            return true;
        }
    }
    error = "Invalid HachiShifter Next project: " + file.getFullPathName();
    return false;
}
}
