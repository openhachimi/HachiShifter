#include "source_transition.hpp"
#include <cmath>
namespace melodyne_re {
TransitionCompatibility evaluate_source_transition(const ComponentBoundary&o,const ComponentBoundary&n,TransitionMode mode,double tol) noexcept {
    TransitionCompatibility r;r.mode=mode;
    if(!o.track_present||!n.track_present||!std::isfinite(o.start_seconds)||!std::isfinite(o.end_seconds)||!std::isfinite(n.start_seconds)||!std::isfinite(n.end_seconds)||!std::isfinite(tol)||tol<0){r.reason="nonfinite/missing track boundary";return r;}
    if(o.identity.source_item!=n.identity.source_item && o.identity.source_item!=0 && n.identity.source_item!=0){r.reason="source identities differ";return r;}
    if(mode==TransitionMode::mode0_following_join){
        if(!n.following_join_present){r.status=TransitionStatus::incompatible;r.reason="followingJoin absent";return r;}
        r.gap_seconds=n.start_seconds-o.end_seconds;r.status=std::abs(r.gap_seconds)<=tol?TransitionStatus::compatible:TransitionStatus::incompatible;
        r.reason=r.status==TransitionStatus::compatible?nullptr:"boundary exceeds tolerance";return r;
    }
    if(mode==TransitionMode::mode1_time_boundary){r.gap_seconds=n.start_seconds-o.end_seconds;r.status=std::abs(r.gap_seconds)<=tol?TransitionStatus::compatible:TransitionStatus::incompatible;r.reason=r.status==TransitionStatus::compatible?nullptr:"boundary exceeds tolerance";return r;}
    r.reason="unsupported transition mode";return r;
}
TransitionDispatch dispatch_source_component(PendingTransitionState&s,const ComponentBoundary&c,TransitionCompatibility compat) noexcept {
    TransitionDispatch r;if(compat.status==TransitionStatus::unsupported){r.error="unsupported compatibility";return r;}
    if(s.dispatched){r.ok=true;r.error="request already dispatched";return r;}
    if(c.identity.component==0){r.error="component identity required";return r;}
    s.dispatched=true;++s.pending_count;r.dispatched=true;r.ok=true;
    if(compat.status==TransitionStatus::incompatible)r.reset_child=true;
    return r;
}
TransitionHandoff choose_transition_handoff(const TransitionFrameState&p,const TransitionFrameState&n,bool compatible,bool full) noexcept {
    TransitionHandoff r;if(p.left_length<=0||p.right_length<=0||n.left_length<=0||n.right_length<=0){r.error="positive frame lengths required";return r;}
    if(!compatible||full||n.source_reset){r.ok=true;r.handoff=OLAHandoff::reset_new_child;r.clear_rolling=true;r.request_left_window=r.request_right_window=true;return r;}
    r.ok=true;r.request_right_window=n.processed_support;r.request_left_window=!p.previous_right_window_done;
    r.handoff=n.processed_support?OLAHandoff::full_rebuild_add_support:OLAHandoff::retain_prefix_add_left_copy_right;return r;
}
} // namespace melodyne_re
