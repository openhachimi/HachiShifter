#include "sampled_function.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace melodyne_re {
namespace {
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
// Volatile stores are deliberate binary64 rounding boundaries, including on
// compilers allowed to contract ordinary multiply/add expressions into FMA.
double add(double a, double b) noexcept { volatile double r = a + b; return r; }
double sub(double a, double b) noexcept { volatile double r = a - b; return r; }
double mul(double a, double b) noexcept { volatile double r = a * b; return r; }
double div(double a, double b) noexcept { volatile double r = a / b; return r; }
SampledFunctionResult success(double x) noexcept { return {true, x, nullptr}; }
SampledFunctionResult unsupported(const char* why) noexcept { return {false, 0.0, why}; }
}
SampledFunctionResult evaluate_sampled_function(const SampledFunctionView& f,
                                               double x, bool extrapolate) noexcept {
    // 181DAA864..890 /181DAA9B2..9C9: missing GNData, empty payload, singleton.
    if (f.values == nullptr || f.count == 0) return success(0.0);
    if (f.count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max() / 4))
        return unsupported("float payload exceeds nonnegative signed32 byte length");
    if (f.count == 1) return success(static_cast<double>(f.values[0]));
    // COMISD treats unordered inputs differently from a C++ ordered comparison;
    // this restricted API does not claim compatibility on NaN/Inf coordinates.
    if (!std::isfinite(x)) return unsupported("nonfinite coordinate");
    if (x <= 0.0 && !extrapolate) return success(static_cast<double>(f.values[0]));
    if (!(f.resolution > 0.0) || !std::isfinite(f.resolution))
        return unsupported("segment requires positive finite resolution");
    const std::int32_t last = static_cast<std::int32_t>(f.count - 1);
    std::int32_t lower = 0, upper = 1;
    if (x > 0.0) {
        // 181DAA915..924: binary64 multiplication, CVTTSD2SI truncation, then
        // signed32 +1. Never replace by floor(x/step) or a floating-point clamp.
        const double product = mul(x, f.resolution);
        if (!std::isfinite(product) || product >= 2147483647.0)
            return unsupported("native signed32 index conversion/addition hazard");
        lower = static_cast<std::int32_t>(product);
        upper = lower + 1;
        if (upper > last) {
            if (!extrapolate) return success(static_cast<double>(f.values[last]));
            upper = last;
            lower = last - 1;
        }
    }
    // 181DAA8B2..8F9: reciprocal FIRST, then index*reciprocal, float->double.
    const double step = div(1.0, f.resolution);
    const double left_x = mul(static_cast<double>(lower), step);
    const double right_x = mul(static_cast<double>(upper), step);
    const double left_y = static_cast<double>(f.values[lower]);
    const double right_y = static_cast<double>(f.values[upper]);
    if (left_x == right_x) return success(left_y);
    // 181DAA94D..9A2: nearest endpoint anchor; strict midpoint < x, so ties
    // choose left. Operation order is difference * dy / dx + endpoint.
    const double dy = sub(right_y, left_y);
    const double dx = sub(right_x, left_x);
    const double midpoint = mul(add(right_x, left_x), 0.5);
    if (midpoint < x)
        return success(add(div(mul(sub(x, right_x), dy), dx), right_y));
    return success(add(div(mul(sub(x, left_x), dy), dx), left_y));
}

SampledSegmentResult evaluate_sampled_segment_forward(
    const SampledFunctionView& f,double x,double dx,int requested,int cache,
    bool extrapolate) noexcept {
    const auto fail=[&](const char* why){return SampledSegmentResult{false,0.0,requested,cache,why};};
    const auto done=[&](double value,int count,int index){
        if(!std::isfinite(value))return fail("nonfinite sampled-segment result");
        return SampledSegmentResult{true,value,count,index,nullptr};
    };
    if(f.values==nullptr || f.count==0)return {true,0.0,requested,cache,nullptr};
    if(f.count<2)return fail("segment accessor requires at least two samples");
    if(f.count>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()/4))
        return fail("payload exceeds native signed32 byte-length domain");
    if(!std::isfinite(x) || !std::isfinite(dx) || !(dx>0.0) ||
       !std::isfinite(f.resolution) || !(f.resolution>0.0))
        return fail("requires finite coordinate, positive step and resolution");
    const double h=div(1.0,f.resolution);
    const int last=static_cast<int>(f.count-1);
    const double last_x=mul(static_cast<double>(last),h);
    if(!(h>0.0) || !std::isfinite(h) || !std::isfinite(last_x))
        return fail("unsupported native knot-coordinate domain");
    int count=requested<1?1:requested;
    const double extent=mul(static_cast<double>(count),dx);
    double end=add(extent,x);
    if(!std::isfinite(extent) || !std::isfinite(end))return fail("nonfinite segment budget extent");
    const auto interpolate=[&](int lo,int hi,double endpoint){
        const double left_x=mul(static_cast<double>(lo),h);
        const double right_x=mul(static_cast<double>(hi),h);
        const double left_y=static_cast<double>(f.values[lo]);
        const double right_y=static_cast<double>(f.values[hi]);
        // 9D0 uses slope FIRST and always the lower anchor. Do not call860.
        const double slope=div(sub(right_y,left_y),sub(right_x,left_x));
        return add(mul(slope,sub(endpoint,left_x)),left_y);
    };
    if(x<0.0){
        bool crosses=false;
        if(end>=0.0){
            const double q=div(sub(0.0,x),dx);
            if(!std::isfinite(q) || q>=2147483647.0)return fail("left crossing count conversion hazard");
            count=static_cast<int>(q)+1;
            if(count<1)count=1;
            crosses=true;
        }
        if(extrapolate || crosses){
            const double endpoint=add(mul(static_cast<double>(count),dx),x);
            return done(interpolate(0,1,endpoint),count,0);
        }
        return done(static_cast<double>(f.values[0]),count,0);
    }
    if(x>last_x){
        // Positive direction forces one sample beyond the right boundary.
        if(extrapolate)return done(interpolate(last-1,last,add(dx,x)),1,last);
        return done(static_cast<double>(f.values[last]),1,last);
    }
    // Literal bounded cache search: at most three adjacent candidate intervals.
    int selected=-1;
    if(cache>=0 && cache<=last){
        if(x<mul(static_cast<double>(cache),h)){
            int lo=cache-1,hi=cache;
            for(int tries=0;tries<3 && lo>=0;++tries,--lo,--hi)
                if(x>=mul(static_cast<double>(lo),h) && mul(static_cast<double>(hi),h)>x){selected=lo;break;}
        } else if(cache<last){
            int lo=cache,hi=cache+1;
            for(int tries=0;tries<3 && lo<last;++tries,++lo,++hi)
                if(x>=mul(static_cast<double>(lo),h) && mul(static_cast<double>(hi),h)>x){selected=lo;break;}
        }
    }
    if(selected==-1){
        if(x==0.0)selected=0;
        else {
            const double q=div(x,h); // NOT x*resolution (860's expression)
            if(!std::isfinite(q) || q<0.0 || q>=2147483647.0)return fail("cache fallback index conversion hazard");
            selected=static_cast<int>(q);
        }
    }
    int hi=last,lo=last-1;
    if(selected+1<=last){lo=selected;hi=selected+1;}
    double right_x=mul(static_cast<double>(hi),h);
    const double first=add(x,dx);
    int crossed_one=0;
    if(first>right_x){
        // Select interval containing x+dx; equality stays in its left interval.
        while(true){
            lo=hi;++hi;
            if(hi>last){
                if(extrapolate)return done(interpolate(last-1,last,first),1,last);
                return done(static_cast<double>(f.values[last]),1,last);
            }
            right_x=mul(static_cast<double>(hi),h);
            if(first<=right_x){crossed_one=1;break;}
        }
    }
    if(end>right_x){
        const double q=div(sub(right_x,x),dx);
        if(!std::isfinite(q) || q<0.0 || q>=2147483647.0)return fail("forward count conversion hazard");
        int steps=static_cast<int>(q);
        if(hi==last && extrapolate){
            if(steps==std::numeric_limits<int>::max())return fail("extrapolated count overflow");
            ++steps;
        }
        if(steps<1)steps=1;
        if(crossed_one && steps==std::numeric_limits<int>::max())return fail("crossing count overflow");
        count=steps+crossed_one;
        // Native endpoint uses steps, NOT the possibly incremented output count.
        end=add(mul(static_cast<double>(steps),dx),x);
    }
    return done(interpolate(lo,hi,end),count,lo);
}
} // namespace melodyne_re
