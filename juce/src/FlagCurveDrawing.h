#pragma once
#include "ProjectModel.h"
#include <algorithm>
#include <cmath>

namespace hachi
{
// Keep whole untouched segments verbatim. Only a curved segment cut by a stroke
// is sampled; its retained part stays within 0.002 parameter units of the source.
inline std::vector<FlagCurvePoint> spliceFlagDrawing(
    const std::vector<FlagCurvePoint>& original, double from, double to,
    const std::vector<FlagCurvePoint>& ink)
{
    if (ink.empty() || original.empty() || to < from) return original;
    constexpr double guard = 0.0001;
    std::vector<FlagCurvePoint> result;
    const auto appendSample = [&](auto&& self, double a, double b, int depth) -> void
    {
        const auto va = flagCurveValueAt(original, a), vb = flagCurveValueAt(original, b);
        auto error = 0.0f;
        for (const auto fraction : {0.25, 0.5, 0.75})
            error = std::max(error, std::abs(flagCurveValueAt(original, a + (b-a)*fraction)
                - static_cast<float>(va + (vb-va)*fraction)));
        if (depth < 18 && b-a > 0.00004 && (error > 0.002f || b-a > 0.05))
        {
            const auto mid = (a+b)*0.5;
            self(self, a, mid, depth+1); self(self, mid, b, depth+1);
        }
        else result.push_back({b, vb, PitchCurveShape::linear});
    };
    const auto left = std::max(-5.0, from-guard), right = std::min(60.0, to+guard);
    for (const auto& point : original)
        if (point.timeSeconds < left) result.push_back(point);
    if (left < from)
    {
        const auto next = std::upper_bound(original.begin(), original.end(), left,
            [](double t, const auto& p) { return t < p.timeSeconds; });
        if (!result.empty() && next != original.end() && next->shape != PitchCurveShape::linear
            && next->shape != PitchCurveShape::natural)
            appendSample(appendSample, result.back().timeSeconds, left, 0);
        else result.push_back({left, flagCurveValueAt(original, left), PitchCurveShape::linear});
    }
    result.push_back({from, flagCurveValueAt(ink, from), PitchCurveShape::linear});
    for (const auto& point : ink)
        if (point.timeSeconds > from+1.0e-5 && point.timeSeconds < to-1.0e-5)
            result.push_back(point);
    if (to > from+1.0e-5)
        result.push_back({to, flagCurveValueAt(ink, to), PitchCurveShape::linear});
    if (right > to) result.push_back({right, flagCurveValueAt(original, right), PitchCurveShape::linear});
    auto first = true;
    for (auto point : original)
        if (point.timeSeconds > right)
        {
            if (first && point.shape != PitchCurveShape::linear && point.shape != PitchCurveShape::natural)
                appendSample(appendSample, right, point.timeSeconds, 0);
            else
            {
                if (first) point.shape = PitchCurveShape::linear;
                result.push_back(point);
            }
            first = false;
        }
    return result;
}
}
