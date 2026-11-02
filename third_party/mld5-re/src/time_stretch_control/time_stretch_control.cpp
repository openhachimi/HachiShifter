#include "../time_stretch_control.hpp"
#include <cmath>
#include <limits>
namespace melodyne_re {
namespace { bool pos(double x){return std::isfinite(x)&&x>0;} }
bool StretchController::initialize(StretchControlState& s,const StretchControlParameters& p,double source,double step){
 if(!pos(p.source_rate)||!pos(p.output_rate)||!pos(step)||!std::isfinite(source)||source<0||p.buffer_capacity<16||(p.stride!=1&&p.stride!=2&&p.stride!=4)||p.bridge632||p.history_path||p.rho!=1.f)return false;
 s.source_position=source;s.source_step=step;s.rate=p.source_rate/(double(p.stride)*p.output_rate);s.rate_delta=0;s.read_position=source-static_cast<double>(static_cast<std::int64_t>(source));s.threshold=1;s.previous_span=1;s.current_span=1;s.source_origin=static_cast<std::int64_t>(source);return true;
}
bool StretchController::advance_output(StretchControlState& s) noexcept {double ns=s.source_position+s.source_step,nr=s.rate+s.rate_delta,nc=s.read_position+nr;if(!std::isfinite(ns)||!std::isfinite(nr)||!std::isfinite(nc))return false;s.source_position=ns;s.rate=nr;s.read_position=nc;return true;}
bool StretchController::rollover(StretchControlState& s,int h) noexcept {if(h<=0||s.current_span<=0||!std::isfinite(s.read_position)||s.read_position<s.threshold)return false;s.read_position-=s.threshold;s.previous_span=s.current_span;s.threshold=s.current_span;s.current_span=h;return true;}
bool StretchController::plan_frame(const StretchControlState& s,const StretchControlParameters& p,const StretchPeriod& period,StretchFrameEvent& e){e={};if(!period||p.bridge632||p.history_path||p.rho!=1.f||p.stride<1||p.stride>4||s.read_position<s.threshold)return false;float q=period(s.source_position/p.source_rate);if(!std::isfinite(q)||q<=0)return false;int h=static_cast<int>(q/static_cast<float>(p.stride)+.5f);if(h<=0||h>p.buffer_capacity/2)return false;e.due=true;e.left=s.current_span;e.right=h;e.source_hops=1;e.full_rebuild=false;e.fetch_origin=s.source_origin+std::int64_t(p.stride)*s.current_span;e.fetch_offset=s.current_span;e.fetch_count=h;return true;}
bool StretchController::read_boxcar(const std::vector<float>& x,std::int64_t origin,int stride,int count,std::vector<float>& out){if(stride<1||stride>4||count<0||origin<0)return false;const std::int64_t end=origin+std::int64_t(stride)*count;if(end<origin||end>static_cast<std::int64_t>(x.size()))return false;out.assign(count,0);for(int i=0;i<count;++i)for(int j=0;j<stride;++j)out[i]+=x[origin+std::int64_t(i)*stride+j];return true;}
}