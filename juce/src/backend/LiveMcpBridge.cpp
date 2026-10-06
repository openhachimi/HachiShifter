#include "LiveMcpBridge.h"
#include <iostream>

namespace hachi::backend
{
namespace
{
juce::var failure(const juce::var& request, int code, const juce::String& message)
{
    auto* error = new juce::DynamicObject();
    error->setProperty("code", code); error->setProperty("message", message);
    auto* result = new juce::DynamicObject();
    result->setProperty("jsonrpc", "2.0"); result->setProperty("id", request["id"]);
    result->setProperty("error", juce::var(error)); return result;
}
juce::Array<juce::var> sessions()
{
    juce::Array<juce::var> result;
    for (const auto& dir : LiveMcpBridge::sessionsDirectory().findChildFiles(juce::File::findDirectories, false))
    {
        auto value = juce::JSON::parse(dir.getChildFile("session.json"));
        const auto age = juce::Time::currentTimeMillis() - static_cast<juce::int64>(value["heartbeat_ms"]);
        if (value.isObject() && age >= 0 && age < 15000) result.add(value);
    }
    return result;
}
}

juce::File LiveMcpBridge::sessionsDirectory()
{
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("HachiShifter-MCP");
}
LiveMcpBridge::LiveMcpBridge(Handler handler, std::function<juce::var()> describe)
    : handle(std::move(handler)), description(std::move(describe)),
      id(juce::Uuid().toString()), folder(sessionsDirectory().getChildFile(id))
{
    folder.getChildFile("in").createDirectory();
    folder.getChildFile("out").createDirectory();
    publish(); startTimer(50);
}
LiveMcpBridge::~LiveMcpBridge()
{
    stopTimer();
    // Only this instance's UUID directory, never other windows' mailboxes.
    folder.deleteRecursively();
}
void LiveMcpBridge::publish()
{
    auto value = description();
    if (auto* o = value.getDynamicObject())
    {
        o->setProperty("session_id", id);
        o->setProperty("heartbeat_ms", juce::Time::currentTimeMillis());
        o->setProperty("version", JUCE_APPLICATION_VERSION_STRING);
        o->setProperty("executable", juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName());
    }
    folder.getChildFile("session.json").replaceWithText(juce::JSON::toString(value, true));
    lastPublish = juce::Time::getMillisecondCounterHiRes();
}
void LiveMcpBridge::timerCallback()
{
    if (juce::Time::getMillisecondCounterHiRes() - lastPublish > 1000) publish();
    auto files = folder.getChildFile("in").findChildFiles(juce::File::findFiles, false, "*.json");
    // Limit work per tick so another client cannot starve the window's input.
    int count = 0;
    for (const auto& file : files)
    {
        // JUCE's atomic writer creates hidden *_temp*.json files in this
        // directory. Only a fully published UUID.json is a request; consuming
        // a temporary file could execute a command before its sender commits it.
        const auto key=file.getFileNameWithoutExtension();
        if(key.length()!=32 || key.retainCharacters("0123456789abcdef")!=key)continue;
        if (++count > 4) break;
        if (file.getSize() > 16 * 1024 * 1024) { file.deleteFile(); continue; }
        const auto envelope = juce::JSON::parse(file);
        if (!file.deleteFile()) continue; // Claim once. A timed-out mutation is never replayed.
        const auto request = envelope["request"];
        bool respond = request.hasProperty("id");
        juce::var response;
        if ((juce::int64)envelope["expires_ms"] < juce::Time::currentTimeMillis())
            response = failure(request, -32001, "Request expired before execution; query the editor again.");
        else if (request.getDynamicObject()==nullptr)
        { respond = true; response = failure(request, -32600, "Invalid JSON-RPC request"); }
        else
        {
            const bool trace=juce::SystemStats::getEnvironmentVariable("HACHI_MCP_TRACE",{})=="1";
            if(trace)std::cerr<<"mcp begin "<<request["id"].toString()<<" "<<request["params"]["name"].toString()<<std::endl;
            try { response = handle(request, respond); }
            catch (const std::exception& e) { response = failure(request, -32603, e.what()); }
            catch (...) { response = failure(request, -32603, "Editor command failed"); }
        }
        if(juce::SystemStats::getEnvironmentVariable("HACHI_MCP_TRACE",{})=="1")std::cerr<<"mcp end "<<request["id"].toString()<<std::endl;
        if (respond)
            folder.getChildFile("out").getChildFile(file.getFileName()).replaceWithText(juce::JSON::toString(response, true));
    }
    // Disconnected clients cannot leave response files indefinitely.
    for (const auto& file : folder.getChildFile("out").findChildFiles(juce::File::findFiles, false))
        if (juce::Time::currentTimeMillis() - file.getLastModificationTime().toMilliseconds() > 60000)
            file.deleteFile();
}
int LiveMcpBridge::runProxy(const juce::String& requested, bool listOnly)
{
    if (listOnly) { std::cout << juce::JSON::toString(sessions(), true) << std::endl; return 0; }
    juce::String bound;
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty()) continue;
        const auto request = juce::JSON::parse(juce::String::fromUTF8(line.data(), (int)line.size()));
        const auto responds = request.hasProperty("id") || request.getDynamicObject()==nullptr;
        auto reply = [&](const juce::var& response) {
            if (responds) std::cout << juce::JSON::toString(response, true).toStdString() << std::endl;
        };
        if (request.getDynamicObject()==nullptr) { reply(failure(request, -32700, "Invalid JSON")); continue; }
        if (bound.isEmpty())
        {
            auto available = sessions();
            // session.json is atomically replaced; a transient missing file is
            // not a closed window. Discovery is read-only and safe to retry.
            for(int attempt=0;attempt<5 && (available.isEmpty() || (requested.isNotEmpty()
                && std::none_of(available.begin(),available.end(),[&](const auto& s){return s["session_id"].toString()==requested;})));++attempt)
            {juce::Thread::sleep(30);available=sessions();}
            if (requested.isNotEmpty())
            {
                for (const auto& s : available) if (s["session_id"].toString() == requested) bound = requested;
            }
            else if (available.size() == 1) bound = available[0]["session_id"].toString();
            if (bound.isEmpty())
            {
                reply(failure(request, -32001, available.isEmpty()
                    ? "No live editor. Open the updated HachiShifter window first."
                    : "Select a window with --session=ID; list windows with --mcp-list-sessions. Available: " + juce::JSON::toString(available, true)));
                continue;
            }
        }
        const auto target = sessionsDirectory().getChildFile(bound);
        if (!target.isDirectory())
        { reply(failure(request, -32001, "Bound editor closed. Reconnect explicitly; no other window was selected.")); continue; }
        const auto key = juce::Uuid().toString() + ".json";
        auto* envelope = new juce::DynamicObject();
        envelope->setProperty("request", request);
        envelope->setProperty("expires_ms", juce::Time::currentTimeMillis() + 30000);
        const auto input = target.getChildFile("in").getChildFile(key);
        if (!input.replaceWithText(juce::JSON::toString(juce::var(envelope), true)))
        { reply(failure(request, -32001, "Cannot write editor mailbox")); continue; }
        if (!responds) continue;
        const auto output = target.getChildFile("out").getChildFile(key);
        const auto until = juce::Time::getMillisecondCounterHiRes() + 35000;
        while (!output.existsAsFile() && target.exists() && juce::Time::getMillisecondCounterHiRes() < until)
        {
            const auto heartbeat=(juce::int64)juce::JSON::parse(target.getChildFile("session.json"))["heartbeat_ms"];
            if(heartbeat>0&&juce::Time::currentTimeMillis()-heartbeat>15000)break;
            juce::Thread::sleep(20);
        }
        if (output.existsAsFile()) { reply(juce::JSON::parse(output)); output.deleteFile(); }
        else
        {
            input.deleteFile();
            reply(failure(request, -32001, "Editor disconnected or request timed out. Outcome may be unknown; query state before retrying."));
        }
    }
    return 0;
}
}
