/*
    SlateX - 2026
*/
#include "Signal.hpp"
#include "../../Engine/Scripting/KakaScheduler.hpp"
#include <algorithm>

uint64_t Signal::SetConnection(sol::function fn) {
    uint64_t id = m_nextConnId++;
    m_connections.push_back({ id, fn, false });
    return id;
}

uint64_t Signal::DoOnce(sol::function fn) {
    uint64_t id = m_nextConnId++;
    m_connections.push_back({ id, fn, true });
    return id;
}

void Signal::RemoveConnection(uint64_t connId) {
    m_connections.erase(
        std::remove_if(m_connections.begin(), m_connections.end(),
            [&](const ConnEntry& C) { return C.id == connId; }),
        m_connections.end()
    );
}

void Signal::Wait() {
    uint64_t id = KakaScheduler::Get().SuspendCurrentTask();
    m_waiters.push_back(id);
}

void Signal::Fire(const std::vector<sol::object>& Args) {
    std::vector<uint64_t> toRemove;

    for (auto& C : m_connections) {
        // each connection runs as its own task — one slow/erroring handler
        // never blocks the others, same model as Roblox's RBXScriptSignal
        KakaScheduler::Get().SpawnWithArgs(C.fn, Args, "SignalConnection", ScriptLayer::Game);

        if (C.once) toRemove.push_back(C.id);
    }
    for (auto id : toRemove) RemoveConnection(id);

    for (auto id : m_waiters)
        KakaScheduler::Get().FulfillRequest(id, Args);
    m_waiters.clear();
}