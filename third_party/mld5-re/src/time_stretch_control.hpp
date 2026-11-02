#pragma once
#include <cstdint>
#include <functional>
#include <vector>
namespace melodyne_re {
struct StretchControlState {
    double source_position=0, source_step=0;
    double rate=0, rate_delta=0;
    double read_position=0, threshold=0;
    int previous_span=0,current_span=0;
    std::int64_t source_origin=0;
};
struct StretchControlParameters {
    double source_rate=0, output_rate=0;
    int stride=1, buffer_capacity=0;
    bool bridge632=false, history_path=false;
    float rho=1.f;
};
struct StretchFrameEvent {
    bool due=false, full_rebuild=false;
    int left=0,right=0,source_hops=0;
    std::int64_t fetch_origin=0;
    int fetch_offset=0,fetch_count=0;
};
using StretchPeriod = std::function<float(double)>;
class StretchController {
public:
    static bool initialize(StretchControlState&,const StretchControlParameters&,double source_position,double source_step);
    static bool advance_output(StretchControlState&) noexcept;
    static bool rollover(StretchControlState&,int new_span) noexcept;
    static bool plan_frame(const StretchControlState&,const StretchControlParameters&,const StretchPeriod&,StretchFrameEvent&);
    static bool read_boxcar(const std::vector<float>& planar,std::int64_t origin,int stride,int count,std::vector<float>& out);
};
} // namespace melodyne_re
