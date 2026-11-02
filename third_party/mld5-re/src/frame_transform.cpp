#include "frame_transform.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "Recovered DSP arithmetic must not use fast-math"
#endif
namespace melodyne_re {
FrameWindowTables::FrameWindowTables(const RaisedCosineKernel& kernel)
    : hann_(kernel.table()) {
    for (std::size_t i=0;i<hann_.size();++i)
        sqrt_hann_[i]=std::sqrt(hann_[i]);
}
namespace {
FrameTransformResult reject(const char* reason) {
    FrameTransformResult r; r.error=reason; return r;
}
bool window(std::vector<float>& channel, int begin, int length,
            const RaisedCosineKernel::Table& table, int table_offset, float span) {
    if(length==0)return true; // native may divide by0, but its zero-length loop skips
    const float step=span/static_cast<float>(length);
    float position=0.0f;
    for(int i=0;i<length;++i) {
        if(!std::isfinite(position) || position<0.0f || position>16384.0f)
            return false;
        const int index=static_cast<int>(position)+table_offset;
        if(index<0 || index>16384)return false;
        auto& sample=channel[static_cast<std::size_t>(begin+i)];
        sample=table[static_cast<std::size_t>(index)]*sample;
        position=position+step; // binary32 accumulated, never direct i*step
    }
    return true;
}
FrameTransformResult frame_transform_impl(
    const std::vector<std::vector<float>>& input,
    const FrameTransformParameters& p,const FrameWindowTables& tables,
    bool from_spectrum) {
    if(p.do_fft && !from_spectrum)return reject("FFT branch is unsupported");
    if(from_spectrum && !p.do_fft)return reject("post-IFFT readout requires FFT path flag");
    if(input.empty() || input.front().size()<2 ||
       input.front().size()>static_cast<std::size_t>(std::numeric_limits<int>::max()/4))
        return reject("invalid channel-buffer shape");
    const int n=static_cast<int>(input.front().size());
    if(p.left_length<0 || p.right_length<0 ||
       static_cast<long long>(p.left_length)+p.right_length>n)
        return reject("invalid frame half lengths");
    if(!std::isfinite(p.ratio) || p.ratio<=0.0f)
        return reject("ratio must be positive and finite");
    for(const auto& channel:input) {
        if(channel.size()!=input.front().size())return reject("unequal channel-buffer sizes");
        for(float sample:channel)if(!std::isfinite(sample))return reject("nonfinite input sample");
    }
    const int total=p.left_length+p.right_length;
    int begin=0,end=total;
    const double ratio=static_cast<double>(p.ratio);
    const double inverse_ratio=1.0/ratio;
    const double scaled_left=static_cast<double>(p.left_length)*ratio;
    const double origin=static_cast<double>(p.left_length)-scaled_left;
    if(p.do_resample && p.ratio<1.0f) {
        begin=static_cast<int>(origin)+1;
        const double right=static_cast<double>(p.right_length)*ratio;
        end=static_cast<int>(right+static_cast<double>(p.left_length))+1;
        if(begin<0 || begin>end || end>n)
            return reject("resample interval outside channel buffer");
    }
    FrameTransformResult r;
    r.channels=input;
    r.left_window_done=p.left_window_done;
    r.right_window_done=p.right_window_done;
    r.resample_begin=begin;r.resample_end=end;
    const auto& table=(p.do_resample || from_spectrum)?tables.sqrt_hann():tables.hann();
    if(p.do_resample) {
        if(p.ratio<1.0f){r.left_window_done=true;r.right_window_done=true;}
        for(auto& channel:r.channels) {
            if(!from_spectrum &&
               (!window(channel,0,p.left_length,table,0,8192.0f) ||
                !window(channel,p.left_length,p.right_length,table,8192,8192.0f)))
                return reject("initial window table index outside supported domain");
            // Native scratch has one zero guard before source, and >=3 after.
            std::vector<float> scratch(static_cast<std::size_t>(n)+4,0.0f);
            std::copy(channel.begin(),channel.end(),scratch.begin()+1);
            std::fill(channel.begin(),channel.end(),0.0f);
            for(int output=begin;output<end;++output) {
                const double offset=static_cast<double>(output)-origin;
                const double position=offset*inverse_ratio;
                if(position>=0.0 && position<=static_cast<double>(n-2)) {
                    const int index=static_cast<int>(position);
                    const float fraction=static_cast<float>(position-static_cast<double>(index));
                    if(p.cubic) {
                        channel[static_cast<std::size_t>(output)]=lagrange_cubic(
                            scratch[index],scratch[index+1],scratch[index+2],scratch[index+3],fraction);
                    } else {
                        const float difference=scratch[index+2]-scratch[index+1];
                        const float scaled=difference*fraction;
                        channel[static_cast<std::size_t>(output)]=scaled+scratch[index+1];
                    }
                }
            }
            if(p.ratio<1.0f && !window(channel,begin,end-begin,table,0,16384.0f))
                return reject("short window table index outside supported domain");
        }
    }
    if(p.left_window_requested && !r.left_window_done) {
        for(auto& channel:r.channels)
            if(!window(channel,0,p.left_length,table,0,8192.0f))
                return reject("final left window index outside supported domain");
        // Native asymmetry: this branch does NOT update *a22.
    }
    if(p.right_window_requested && !r.right_window_done) {
        for(auto& channel:r.channels)
            if(!window(channel,p.left_length,p.right_length,table,8192,8192.0f))
                return reject("final right window index outside supported domain");
        r.right_window_done=true;
    }
    for(const auto& channel:r.channels)
        for(float sample:channel)if(!std::isfinite(sample))return reject("frame arithmetic overflow");
    r.ok=true;return r;
}
} // namespace
FrameTransformResult transform_frame_no_fft(
    const std::vector<std::vector<float>>& input,
    const FrameTransformParameters& p,const FrameWindowTables& tables) {
    return frame_transform_impl(input,p,tables,false);
}
FrameTransformResult transform_frame_post_ifft(
    const std::vector<std::vector<float>>& input,
    const FrameTransformParameters& p,const FrameWindowTables& tables) {
    return frame_transform_impl(input,p,tables,true);
}
} // namespace melodyne_re
