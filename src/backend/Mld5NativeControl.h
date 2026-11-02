#pragma once
#include "period_renderer.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace hachi::backend
{
// Hachi's explicit D1, component-relative control adapter; not a reconstruction
// of the application's unrecovered control manager. Period selection, startup,
// windows, FFT, source reads and rolling C remain exclusively library-owned.
// One instance belongs to one component (or one proven compatible source session).
class Mld5NativeControl final : public melodyne_re::detail::PeriodRendererRateControl
{
public:
    struct Parameters
    {
        double sourceRate = 32000., outputRate = 48000., duration = 0.;
        // Sorted ORIGINAL double target seconds, including map AND control knots.
        // Optional 0/end entries are allowed. Never round/move author anchors.
        std::vector<double> targetBoundaries;
        std::function<double(double)> sourceAt;       // target seconds -> source seconds
        std::function<double(double)> pitchRatioAt;   // target seconds -> pitch multiplier
        std::function<float(double)> probeRatioAt;    // same target clock -> actual Q
        int sourceBudget = 139;
        bool processedFrames = false; // optional renderer virtual; no DSP here
        std::function<bool()> cancelled;
    };

    explicit Mld5NativeControl(Parameters parameters) : parameters_(std::move(parameters)) {}
    const char* failureReason() const noexcept { return failure_; }
    int remaining() const noexcept { return remaining_; }
    std::size_t refreshCount() const noexcept { return refreshes_; }
    std::size_t nextOutputIndex() const noexcept { return nextOutput_; }
    double outputTime() const noexcept { return timeAt(nextOutput_); }
    // Deliberately valid with both old and extended library headers. The parent
    // adapter may add this same virtual to the base to fix full-component FFT
    // topology. Q itself is NEVER altered to force a topology decision.
    bool requires_processed_frames() const noexcept { return parameters_.processedFrames; }
    float frame_probe_ratio(float) const noexcept override { return frameQ_; }

    bool initialize(melodyne_re::PeriodStartupParameters& startup, const char*& error) noexcept override
    {
        error = nullptr;
        if (failure_) { error = failure_; return false; }
        if (initialized_) return fail("MLD5 controller cannot restart an active component", error);
        const auto& p = parameters_;
        if (!positive(p.sourceRate) || !positive(p.outputRate) || !positive(p.duration)
            || p.sourceBudget < 1 || p.sourceBudget > 100000000 || !p.sourceAt || startup.source_stride != 1)
            return fail("MLD5 controller requires finite positive rates/duration/budget, D1 and a source map", error);
        const double samples = p.duration * p.outputRate;
        if (!positive(samples) || samples > 100000000.)
            return fail("MLD5 controller component output count exceeds safety limit", error);
        // Suppress only arithmetic noise in an intended integral duration*Fo.
        const double nearest = std::round(samples);
        const double tolerance = 16. * std::numeric_limits<double>::epsilon() * std::max(1., samples);
        const double count = std::abs(samples - nearest) <= tolerance ? nearest : std::ceil(samples);
        if (count < 1.) return fail("MLD5 controller has no component output", error);
        totalOutputs_ = static_cast<std::size_t>(count);
        baseRate_ = p.sourceRate / p.outputRate;
        if (!positive(baseRate_)) return fail("MLD5 neutral rate overflow", error);
        if (!checkCancellation(error)) return false;
        double source = 0., pitch = 1.;
        if (!evaluateSource(0., source, error) || !evaluatePitch(0., pitch, error)
            || !evaluateQ(0., frameQ_, error)) return false;
        initialSource_ = source * p.sourceRate;
        initialRate_ = baseRate_ * double(float(pitch)); // native local pitch float, global1
        if (!sourceCoordinate(initialSource_) || !positive(initialRate_))
            return fail("MLD5 initial source/rate overflow", error);
        // Validate all explicitly supplied anchors without sorting or repairing.
        // The caller supplies the full forward map; arbitrary callbacks cannot
        // be globally proved monotonic by any finite number of observations.
        double previousTime = -1., previousSource = source;
        for (double boundary : p.targetBoundaries)
        {
            if (!std::isfinite(boundary) || boundary < 0. || boundary > p.duration || boundary <= previousTime)
                return fail("MLD5 target anchors must be finite, ordered and inside the component", error);
            double value = 0.;
            if (!evaluateSource(boundary, value, error)) return false;
            if ((boundary == 0. && value != source) || (boundary > 0. && value <= previousSource))
                return fail("MLD5 source map is not strictly forward at its anchors", error);
            previousTime = boundary; previousSource = value;
        }
        double endSource = 0.;
        if (!evaluateSource(p.duration, endSource, error)) return false;
        if (endSource <= source || (previousTime < p.duration && endSource <= previousSource))
            return fail("MLD5 source map has a nonpositive endpoint span", error);
        // Explicit pre-source refresh: positive dS is needed by native startup.
        // It consumes no output and changes neither S nor R. First before_output
        // refreshes the now-real clock again after startup, never resetting C.
        double bootstrapStep = 0., bootstrapDelta = 0.;
        if (!calculateRamp(0, initialSource_, initialRate_, bootstrapStep, bootstrapDelta, error)) return false;
        startup.source_position = initialSource_; startup.source_step = bootstrapStep;
        startup.rate = initialRate_; startup.rate_delta = 0.;
        startup.q = frameQ_; startup.target_pitch_ratio = float(pitch);
        lastSource_ = initialSource_; lastRate_ = initialRate_;
        initialized_ = true;
        return true;
    }

    bool before_output(melodyne_re::PeriodClockState& clock, std::size_t outputIndex,
                       std::size_t remainingOutputs, const char*& error) noexcept override
    {
        error = nullptr;
        if (failure_) { error = failure_; return false; }
        if (!initialized_ || awaitingAfter_ || outputIndex != nextOutput_ || remainingOutputs == 0
            || nextOutput_ >= totalOutputs_)
            return fail("MLD5 output clock must advance once per global component sample", error);
        if (!checkCancellation(error)) return false;
        if (!validClock(clock) || clock.source_position != lastSource_ || clock.rate != lastRate_)
            return fail("MLD5 source/R clock was reset or became invalid between outputs", error);
        if (!evaluateQ(timeAt(nextOutput_), frameQ_, error)) return false;
        if (remaining_ == 0)
        {
            if (!calculateRamp(nextOutput_, clock.source_position, clock.rate,
                               sourceStep_, rateDelta_, error)) return false;
            remaining_ = budgetAt(nextOutput_);
            ++refreshes_;
        }
        // No per-sample S/R assignment, and NEVER a C assignment. remainingOutputs
        // may be a host chunk tail: it must not change the component's ramp.
        clock.source_step = sourceStep_; clock.rate_delta = rateDelta_;
        beforeSource_ = clock.source_position; beforeRate_ = clock.rate;
        beforeRead_ = clock.read_position; beforeThreshold_ = clock.threshold;
        awaitingAfter_ = true;
        return true;
    }

    bool after_output(const melodyne_re::PeriodClockState& clock, const char*& error) noexcept override
    {
        error = nullptr;
        if (failure_) { error = failure_; return false; }
        if (!initialized_ || !awaitingAfter_ || remaining_ < 1)
            return fail("MLD5 output consumed without a current control segment", error);
        // Native: S+=dS, R+=dR, C+=UPDATED_R. There can be one library-owned
        // frame rollover (C-=old threshold) between before and after callbacks.
        const double expectedSource = add(beforeSource_, sourceStep_);
        const double expectedRate = add(beforeRate_, rateDelta_);
        const double directRead = add(beforeRead_, expectedRate);
        const double rolledRead = add(sub(beforeRead_, beforeThreshold_), expectedRate);
        if (!validClock(clock) || expectedSource <= beforeSource_ || !positive(expectedRate)
            || clock.source_position != expectedSource || clock.rate != expectedRate
            || clock.source_step != sourceStep_ || clock.rate_delta != rateDelta_
            || (clock.read_position != directRead
                && !(beforeRead_ >= beforeThreshold_ && clock.read_position == rolledRead)))
            return fail("MLD5 output advance reversed, overflowed, reset C or used stale R", error);
        lastSource_ = clock.source_position; lastRate_ = clock.rate;
        --remaining_; ++nextOutput_; awaitingAfter_ = false;
        return checkCancellation(error);
    }

private:
    Parameters parameters_;
    const char* failure_ = nullptr;
    bool initialized_ = false, awaitingAfter_ = false;
    int remaining_ = 0;
    std::size_t nextOutput_ = 0, totalOutputs_ = 0, refreshes_ = 0;
    double baseRate_ = 0., initialSource_ = 0., initialRate_ = 0.;
    double lastSource_ = 0., lastRate_ = 0., sourceStep_ = 0., rateDelta_ = 0.;
    double beforeSource_ = 0., beforeRate_ = 0., beforeRead_ = 0., beforeThreshold_ = 0.;
    float frameQ_ = 1.f;
    static bool positive(double value) noexcept { return std::isfinite(value) && value > 0.; }
    static bool sourceCoordinate(double value) noexcept
    { return std::isfinite(value) && value >= 0. && value < 0x1p62; }
    static double add(double a, double b) noexcept { volatile double r = a + b; return r; }
    static double sub(double a, double b) noexcept { volatile double r = a - b; return r; }
    double timeAt(std::size_t index) const noexcept { return double(index) / parameters_.outputRate; }
    bool fail(const char* why, const char*& error) noexcept { failure_ = why; error = why; return false; }
    static bool validClock(const melodyne_re::PeriodClockState& s) noexcept
    {
        return sourceCoordinate(s.source_position) && positive(s.source_step) && positive(s.rate)
            && std::isfinite(s.rate_delta) && std::isfinite(s.read_position)
            && s.read_position >= 0. && s.read_position < 0x1p62 && positive(s.threshold);
    }
    bool checkCancellation(const char*& error) noexcept
    {
        try { if (parameters_.cancelled && parameters_.cancelled()) return fail("MLD5 render cancelled", error); }
        catch (...) { return fail("MLD5 cancellation callback failed", error); }
        return true;
    }
    bool evaluateSource(double time, double& value, const char*& error) noexcept
    {
        try { value = parameters_.sourceAt(time); }
        catch (...) { return fail("MLD5 source-map callback failed", error); }
        if (!std::isfinite(value) || value < 0. || !sourceCoordinate(value * parameters_.sourceRate))
            return fail("MLD5 source map returned invalid source time", error);
        return true;
    }
    bool evaluatePitch(double time, double& value, const char*& error) noexcept
    {
        try { value = parameters_.pitchRatioAt ? parameters_.pitchRatioAt(time) : 1.; }
        catch (...) { return fail("MLD5 pitch callback failed", error); }
        if (!positive(value) || !positive(double(float(value))) || !positive(baseRate_ * double(float(value))))
            return fail("MLD5 pitch callback returned invalid rate", error);
        return true;
    }
    bool evaluateQ(double time, float& value, const char*& error) noexcept
    {
        try { value = parameters_.probeRatioAt ? parameters_.probeRatioAt(time) : 1.f; }
        catch (...) { return fail("MLD5 Q callback failed", error); }
        if (!positive(value)) return fail("MLD5 Q callback returned invalid probe ratio", error);
        return true;
    }
    int budgetAt(std::size_t index) const noexcept
    {
        int count = int(std::min<std::size_t>(totalOutputs_ - index, std::size_t(parameters_.sourceBudget)));
        const double now = timeAt(index);
        const auto next = std::upper_bound(parameters_.targetBoundaries.begin(), parameters_.targetBoundaries.end(), now);
        if (next == parameters_.targetBoundaries.end()) return count;
        // Largest integer endpoint NOT after the original double anchor. A
        // fractional anchor cannot coincide with an output sample: stop at its
        // preceding sample, then isolate its one straddling sample (N=1). That
        // sample evaluates sourceAt on the ORIGINAL map at its real endpoint;
        // neither anchor time nor slope is snapped to a different user key.
        // No multi-output budget crosses an anchor. Do not use ceil(distance)
        // to move a key and silently average several samples across it.
        if (timeAt(index + std::size_t(count)) <= *next) return count;
        int low = 1, high = count;
        while (low < high)
        {
            const int middle = low + (high - low + 1) / 2;
            if (timeAt(index + std::size_t(middle)) <= *next) low = middle;
            else high = middle - 1;
        }
        return low;
    }
    bool calculateRamp(std::size_t index, double currentSource, double currentRate,
                       double& step, double& delta, const char*& error) noexcept
    {
        const int count = budgetAt(index);
        if (count < 1) return fail("MLD5 refresh has no remaining component outputs", error);
        const double endpoint = timeAt(index + std::size_t(count));
        double source = 0., pitch = 1.;
        if (!evaluateSource(endpoint, source, error) || !evaluatePitch(endpoint, pitch, error)) return false;
        const double targetSource = source * parameters_.sourceRate;
        const double targetRate = baseRate_ * double(float(pitch));
        step = sub(targetSource, currentSource) / double(count); // S uses integer->double
        delta = sub(targetRate, currentRate) / double(float(count)); // native R count->float
        if (!positive(step) || !std::isfinite(delta) || add(currentSource, step) <= currentSource
            || !positive(add(currentRate, delta)))
            return fail("MLD5 source map reversed or source/rate ramp overflowed", error);
        return true;
    }
};
}
