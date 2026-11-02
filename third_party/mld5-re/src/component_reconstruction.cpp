#include "component_reconstruction.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "Recovered DSP arithmetic must not use fast-math"
#endif

namespace melodyne_re {
namespace {
bool nonnegative_finite(float x) { return std::isfinite(x) && x >= 0.0f; }
ComponentReconstructionResult reject(const char* reason) {
    ComponentReconstructionResult result;
    result.error = reason;
    return result;
}
} // namespace

ComponentReconstructionResult reconstruct_component_mask(
    const std::vector<float>& source, const std::vector<float>& previous,
    const ComponentReconstructionParameters& p, const RaisedCosineKernel& kernel,
    const std::vector<float>& envelope) {
    if (p.unsupported_branches || p.blend_bias != 0.0f ||
        p.channel_shape_amount != 0.0f || p.redistribution_amount != 0.0f ||
        p.energy_exponent != 1.0f || p.harmonic_tilt != 1.0f ||
        p.envelope_exponent != 1.0f || p.output_cap != 1.0f)
        return reject("unsupported non-default reconstruction branch");
    if (source.empty() || source.size() != previous.size() ||
        source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()/4))
        return reject("invalid source/previous-mask size");
    if (!envelope.empty() && envelope.size() != 512)
        return reject("harmonic envelope must have exactly 512 entries");
    if (!std::isfinite(p.spacing_numerator) || p.spacing_numerator <= 0.0f ||
        !std::isfinite(p.spacing_denominator) || p.spacing_denominator <= 0.0f ||
        !nonnegative_finite(p.envelope_probe_ratio) ||
        !nonnegative_finite(p.output_gain))
        return reject("invalid reconstruction parameter");
    const int n = static_cast<int>(source.size());
    const float d = p.spacing_numerator / p.spacing_denominator;
    const float inverse_d = 1.0f / d;
    // Defensive subset: no original overflow/saturated pointer arithmetic.
    if (!std::isfinite(d) || d < std::numeric_limits<float>::min() || d > static_cast<float>(n) ||
        !std::isfinite(inverse_d))
        return reject("spacing outside supported finite indexing domain");
    for (float x : source)
        if (!nonnegative_finite(x)) return reject("invalid source magnitude");
    for (float x : previous)
        if (!nonnegative_finite(x)) return reject("invalid previous mask");
    for (float x : envelope)
        if (!nonnegative_finite(x)) return reject("invalid harmonic envelope");
    const float components = static_cast<float>(n) * inverse_d;
    const int last = components >= 1023.0f ? 1023 : static_cast<int>(std::ceil(components));
    const int radius = static_cast<int>(std::ceil(d));
    const int width = 2 * radius + 1;
    const float max_center = static_cast<float>(last) * d;
    const float max_probe = p.envelope_probe_ratio * max_center;
    if (!std::isfinite(max_probe) || max_probe > static_cast<float>(std::numeric_limits<int>::max()/2))
        return reject("probe outside supported finite indexing domain");
    ComponentReconstructionResult r;
    r.last_component = last;
    r.target_magnitude.assign(source.size(), 0.0f);
    r.mask = previous;
    std::vector<float> taps(static_cast<std::size_t>(width));
    constexpr float minimum = std::numeric_limits<float>::min();
    const bool probe = p.envelope_probe_ratio < 0.999f || p.envelope_probe_ratio > 1.001f;
    for (int h = 0; h <= last; ++h) {
        const float center = static_cast<float>(h) * d;
        const int start = round_mode0(center) - radius;
        float mass = 0.0f;
        for (int t = 0; t < width; ++t) {
            const int bin = start + t;
            float value = 0.0f;
            if (bin > 0 && bin < n) {
                const float distance = std::fabs(static_cast<float>(bin) - center);
                if (d > distance) {
                    const float x = 1.0f - distance * inverse_d;
                    value = kernel.lookup(x) * source[static_cast<std::size_t>(bin)];
                }
            }
            taps[static_cast<std::size_t>(t)] = value;
            mass = mass + value;
        }
        float gain = envelope.empty() ? 1.0f : envelope[static_cast<std::size_t>(std::min(h,511))];
        if (probe && mass >= minimum) {
            const float probe_center = p.envelope_probe_ratio * center;
            const int probe_start = round_mode0(probe_center) - radius;
            float probe_mass = 0.0f;
            for (int t = 0; t < width; ++t) {
                const int bin = probe_start + t;
                if (bin > 0 && bin < n) {
                    const float distance = std::fabs(static_cast<float>(bin) - probe_center);
                    if (d > distance) {
                        const float x = 1.0f - distance * inverse_d;
                        const float value = kernel.lookup(x) * source[static_cast<std::size_t>(bin)];
                        probe_mass = probe_mass + value;
                    }
                }
            }
            gain = gain * (probe_mass / mass);
        }
        for (int t = 0; t < width; ++t) {
            const int bin = start + t;
            if (bin >= 0 && bin < n) {
                const float value = gain * taps[static_cast<std::size_t>(t)];
                auto& target = r.target_magnitude[static_cast<std::size_t>(bin)];
                target = value + target;
            }
        }
    }
    for (int bin = 1; bin < n; ++bin) {
        r.source_sum = r.source_sum + source[static_cast<std::size_t>(bin)];
        r.target_sum = r.target_sum + r.target_magnitude[static_cast<std::size_t>(bin)];
    }
    if (!std::isfinite(r.source_sum) || !std::isfinite(r.target_sum))
        return reject("intermediate magnitude overflow outside supported domain");
    const float ratio = r.target_sum >= minimum ? r.source_sum / r.target_sum : 1.0f;
    r.normalization = std::fmin(100.0f, ratio);
    const float scale = r.normalization * p.output_gain;
    if (!std::isfinite(scale)) return reject("output scale overflow outside supported domain");
    for (int bin = 0; bin < n; ++bin) {
        const auto b = static_cast<std::size_t>(bin);
        float value = source[b] < minimum ? previous[b] : r.target_magnitude[b] / source[b];
        if (r.target_magnitude[b] < minimum) value = 0.0f;
        r.mask[b] = scale * value;
        if (!std::isfinite(r.mask[b])) return reject("mask overflow outside supported domain");
    }
    r.ok = true;
    return r;
}
} // namespace melodyne_re
