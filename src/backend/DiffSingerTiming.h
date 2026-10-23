#pragma once
#include <algorithm>
#include <vector>

namespace hachi::backend
{
// The same endpoint mapping in the mixer and the envelope editor. Preserve
// the attack/release ramp lengths when the phoneme onset moves before the beat.
template<class Point>
std::vector<Point> diffSingerEnvelope(std::vector<Point> points, double from, double to)
{
    if (points.size() < 2) return points;
    const auto startShift = from - points.front().timeSeconds;
    const auto endShift = to - points.back().timeSeconds;
    if (points.size() >= 4)
    {
        points[1].timeSeconds += startShift;
        points[points.size()-2].timeSeconds += endShift;
    }
    points.front().timeSeconds = from;
    points.back().timeSeconds = to;
    for (std::size_t i=1; i<points.size(); ++i)
        points[i].timeSeconds = std::clamp(points[i].timeSeconds, points[i-1].timeSeconds, to);
    return points;
}
}
