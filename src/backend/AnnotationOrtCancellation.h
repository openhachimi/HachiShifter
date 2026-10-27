#pragma once
#include "JapaneseAnnotationAudio.h"
#include "OrtExecution.h"
#include <atomic>
#include <thread>

namespace hachi::backend
{
#if defined(HACHI_HAS_ONNX_ANALYSIS) && HACHI_HAS_ONNX_ANALYSIS
// Owned by the inference call. No detached work; session/options outlive watcher.
class AnnotationOrtCancellation final
{
public:
    AnnotationOrtCancellation(Ort::RunOptions& options, AnnotationCancel cancel)
    {
        if (cancel) watcher = std::thread([this, &options, cancel = std::move(cancel)]
        {
            while (!finished.load(std::memory_order_acquire))
            {
                if (cancel()) { try { options.SetTerminate(); } catch (...) {} return; }
                juce::Thread::sleep(10);
            }
        });
    }
    ~AnnotationOrtCancellation()
    { finished.store(true, std::memory_order_release); if (watcher.joinable()) watcher.join(); }
private:
    std::atomic<bool> finished {false};
    std::thread watcher;
};
#endif
}
