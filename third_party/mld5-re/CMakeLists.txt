# User-supplied G:/mld5 core plus WSL melodyne5-re transition/stretch modules (2026-11-01).
# Production C++ only: no original DLL, model, reference audio or research tools.
add_library(hachi_mld5_re STATIC
    src/primitives.cpp src/component_reconstruction.cpp src/packed_fft.cpp
    src/frame_transform.cpp src/spectral_frame.cpp src/period_clock.cpp
    src/sampled_function.cpp src/unpitched_renderer.cpp src/preparation.cpp
    src/pitch_cache.cpp src/period_renderer.cpp src/cached_control.cpp
    src/source_time_control.cpp src/output_gain_control.cpp
    src/source_transition.cpp src/time_stretch_control/time_stretch_control.cpp)
target_compile_features(hachi_mld5_re PUBLIC cxx_std_17)
target_include_directories(hachi_mld5_re PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(hachi_mld5_re PROPERTIES INTERPROCEDURAL_OPTIMIZATION FALSE)
# PRIVATE deliberately: don't alter floating-point semantics of other engines.
if(MSVC)
    target_compile_options(hachi_mld5_re PRIVATE /fp:strict)
else()
    target_compile_options(hachi_mld5_re PRIVATE -fno-fast-math -ffp-contract=off)
endif()
