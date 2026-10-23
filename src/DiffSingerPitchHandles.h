#pragma once
#include "DiffSingerPitchRestore.h"

namespace hachi
{
// A frame remains in the curve used for rendering. Only its editor handle is
// hidden, so switching tools or opening an older project cannot alter its sound.
inline std::vector<PitchCurveEditPoint> compactDiffSingerPitchHandles(
    std::vector<PitchCurveEditPoint> points,
    const std::vector<PitchCurveEditPoint>& reference, double duration,
    bool preserveOutside = false)
{
    if (reference.empty()) return points;
    const auto maximum = static_cast<size_t>(juce::jlimit(2, 16,
        static_cast<int>(std::floor(duration / .045)) + 2));
    std::vector<size_t> automatic;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto& p = points[i];
        if (p.diffSingerRestoreSupport || (preserveOutside
            && (p.timeSeconds < 0 || p.timeSeconds > duration))) continue;
        const auto r = std::lower_bound(reference.begin(), reference.end(), p.timeSeconds - 1e-8,
            [](const auto& point, double time) { return point.timeSeconds < time; });
        // A manually moved, inserted or reshaped point must remain visible.
        if (r != reference.end() && std::abs(r->timeSeconds - p.timeSeconds) < 1e-7
            && std::abs(r->targetMidi - p.targetMidi) < 1e-5f && r->shape == p.shape
            && r->bezierX1 == p.bezierX1 && r->bezierY1 == p.bezierY1
            && r->bezierX2 == p.bezierX2 && r->bezierY2 == p.bezierY2)
            automatic.push_back(i);
    }
    if (automatic.size() <= maximum) return points;
    // Keep endpoints and progressively keep the most significant bends. The
    // same 2.5-cent starting tolerance and 16-handle cap as ordinary UTAU.
    std::vector<size_t> kept{0, automatic.size() - 1};
    while (kept.size() < maximum)
    {
        auto worst = .025f; size_t chosen = automatic.size();
        for (size_t k = 1; k < kept.size(); ++k)
        {
            const auto& left = points[automatic[kept[k - 1]]];
            const auto& right = points[automatic[kept[k]]];
            const auto span = right.timeSeconds - left.timeSeconds;
            for (size_t j = kept[k - 1] + 1; j < kept[k]; ++j)
            {
                const auto& p = points[automatic[j]];
                const auto amount = span > 1e-9 ? (p.timeSeconds - left.timeSeconds) / span : 0.;
                const auto error = std::abs(p.targetMidi - static_cast<float>(
                    left.targetMidi + (right.targetMidi - left.targetMidi) * amount));
                if (error > worst) { worst = error; chosen = j; }
            }
        }
        if (chosen == automatic.size()) break;
        kept.insert(std::lower_bound(kept.begin(), kept.end(), chosen), chosen);
    }
    for (const auto i : automatic) points[i].diffSingerRestoreSupport = true;
    for (const auto i : kept) points[automatic[i]].diffSingerRestoreSupport = false;
    return points;
}

// Editing a visible handle addresses the span between visible neighbours, not
// the invisible 5-ms samples. Keep the rest of the curve, including its tangents.
inline void prepareSampledPitchPointEdit(std::vector<PitchCurveEditPoint>& points, int& index)
{
    if (index < 0 || index >= static_cast<int>(points.size())) return;
    auto left = index - 1, right = index + 1;
    while (left > 0 && points[static_cast<size_t>(left)].diffSingerRestoreSupport) --left;
    while (right + 1 < static_cast<int>(points.size())
        && points[static_cast<size_t>(right)].diffSingerRestoreSupport) ++right;
    if (left == index - 1 && right == index + 1) return;
    std::vector<PitchCurveEditPoint> result;
    for (int i = 0; i <= left; ++i)
        result.push_back(i == left ? frozenPitchSegment(points, static_cast<size_t>(i)) : points[static_cast<size_t>(i)]);
    const auto originalIndex = index;
    auto active = points[static_cast<size_t>(index)];
    active.diffSingerRestoreSupport = false;
    if (left != originalIndex - 1) active.shape = PitchCurveShape::natural;
    index = static_cast<int>(result.size()); result.push_back(active);
    if (right < static_cast<int>(points.size()))
    {
        auto endpoint = points[static_cast<size_t>(right)];
        if (right != originalIndex + 1) endpoint.shape = PitchCurveShape::natural;
        result.push_back(endpoint);
        for (size_t i = static_cast<size_t>(right + 1); i < points.size(); ++i)
            result.push_back(i == static_cast<size_t>(right + 1) ? frozenPitchSegment(points, i) : points[i]);
    }
    points = std::move(result);
}
}
