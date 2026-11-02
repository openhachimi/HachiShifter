#pragma once
#include <cstdint>
#include <string>
namespace melodyne_re {
enum class TransitionStatus { compatible, incompatible, unsupported };
enum class TransitionMode { mode0_following_join, mode1_time_boundary };
enum class OLAHandoff { retain_prefix_add_left_copy_right, full_rebuild_add_support, incremental_copy_right, reset_new_child };
struct SourceIdentity { std::int64_t source_item=0, element=0, component=0; };
struct ComponentBoundary { SourceIdentity identity; bool track_present=true; bool following_join_present=false; double start_seconds=0,end_seconds=0; };
struct TransitionCompatibility { TransitionStatus status=TransitionStatus::unsupported; TransitionMode mode=TransitionMode::mode0_following_join; double gap_seconds=0; const char* reason=nullptr; };
TransitionCompatibility evaluate_source_transition(const ComponentBoundary& old_component,const ComponentBoundary& new_component,TransitionMode mode,double tolerance_seconds=1.0/44100.0) noexcept;
struct PendingTransitionState { bool dispatched=false; std::uint32_t pending_count=0; bool current_active=false; };
struct TransitionDispatch { bool ok=false; bool dispatched=false; bool popped=false; bool reset_child=false; const char* error=nullptr; };
TransitionDispatch dispatch_source_component(PendingTransitionState& state,const ComponentBoundary& component,TransitionCompatibility compatibility) noexcept;
struct TransitionFrameState { int left_length=0,right_length=0; bool previous_right_window_done=false; bool source_reset=false; bool processed_support=false; };
struct TransitionHandoff { bool ok=false; OLAHandoff handoff=OLAHandoff::reset_new_child; bool request_left_window=false,request_right_window=false; bool clear_rolling=false; const char* error=nullptr; };
TransitionHandoff choose_transition_handoff(const TransitionFrameState& previous,const TransitionFrameState& next,bool compatible,bool full_frame_required) noexcept;
} // namespace melodyne_re
