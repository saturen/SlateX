/*
    SlateX - 2026
*/
#pragma once
#include <sol/sol.hpp>
#include <vector>
#include <cstdint>

// Signal — generic event primitive. Backs NetworkEvent's Fired/OnServer/
// OnClient members, and anything else that needs the same contract:
//
//   :SetConnection(fn) -> connId   persistent listener, runs as its own task
//   :DoOnce(fn)        -> connId   same, but removes itself after one Fire
//   :Wait()            -> ...args  suspends current task until next Fire
//   :Fire(...args)                 invokes connections + resolves waiters
//
// Each connection (and each Wait()) runs in its own coroutine via
// KakaScheduler::Spawn, so a slow/erroring handler never blocks others —
// same model as Roblox's RBXScriptSignal.
class Signal {
public:
    uint64_t SetConnection(sol::function fn);
    uint64_t DoOnce(sol::function fn);
    void     RemoveConnection(uint64_t connId);

    // Must be called from inside a sol::yielding wrapper — registers the
    // current task as a waiter and returns immediately; the actual "return
    // value" of :Wait() in Lua comes later, when Fire() resumes it.
    void Wait();

    void Fire(const std::vector<sol::object>& Args);

    // Returns the first registered connection's function, or an invalid
    // sol::function if none. Used for Type::Function NetworkEvents, where
    // OnServer/OnClient conceptually behave like Roblox's single-callback
    // OnServerInvoke/OnClientInvoke rather than a multi-listener signal —
    // if more than one connection is registered, only the first answers.
    sol::function GetFirstHandler() const {
        return m_connections.empty() ? sol::function() : m_connections.front().fn;
    }

private:
    struct ConnEntry {
        uint64_t     id;
        sol::function fn;
        bool         once;
    };

    std::vector<ConnEntry> m_connections;
    std::vector<uint64_t>  m_waiters;
    uint64_t               m_nextConnId = 1;
};