#pragma once
#include <juce_events/juce_events.h>
#include <functional>

namespace hachi::backend
{
// Local, per-window mailbox. Requests are atomically published and consumed on
// the message thread. No socket, global project, or stale GUI pointer is shared.
class LiveMcpBridge final : private juce::Timer
{
public:
    using Handler = std::function<juce::var(const juce::var&, bool&)>;
    LiveMcpBridge(Handler handler, std::function<juce::var()> describe);
    ~LiveMcpBridge() override;
    const juce::String& sessionId() const { return id; }
    static int runProxy(const juce::String& session = {}, bool listOnly = false);
    static juce::File sessionsDirectory();
private:
    void timerCallback() override;
    void publish();
    Handler handle;
    std::function<juce::var()> description;
    juce::String id;
    juce::File folder;
    double lastPublish = 0;
};
}
