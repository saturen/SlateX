/*
    SlateX - 2026
*/
#pragma once
#include <sol/sol.hpp>
#include <list>
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include "LuaSandbox.hpp"

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

struct ENGINE_API KakaTask {
    sol::thread    thread;
    sol::coroutine co;
    double         resumeAt;
    std::string    name;
    ScriptLayer    layer;

    // request-based suspension (used by NetworkEvent::InvokeServer/InvokeClient).
    // when waitingForRequest is true, this task is NOT resumed by time —
    // it's resumed only via FulfillRequest() with the matching requestId.
    bool                      waitingForRequest = false;
    bool                      responseReady     = false;
    uint64_t                  requestId         = 0;
    std::vector<sol::object>  pendingResponseArgs;

    // fired once when this task finishes normally (status == ok), with its
    // return values — used by NetworkEvent to answer an incoming Invoke
    // after the handler runs (and possibly wait()s) to completion.
    std::function<void(std::vector<sol::object>)> onComplete;
};

class ENGINE_API KakaScheduler {
public:
    static KakaScheduler& Get();

    void Init(sol::state& lua);
    void Tick(double now);

    void Spawn(sol::function fn,
               double delay            = 0.0,
               const std::string& name = "",
               ScriptLayer layer       = ScriptLayer::Game);

    // Like Spawn(), but delivers InitialArgs as the function's first call
    // arguments (reuses the same delivery path as request fulfillment —
    // a fresh coroutine's first resume just receives args as normal params).
    // Used by Signal::Fire() to invoke connections with the fired arguments.
    void SpawnWithArgs(sol::function fn,
                        std::vector<sol::object> initialArgs,
                        const std::string& name = "",
                        ScriptLayer layer       = ScriptLayer::Game);

    // Like SpawnWithArgs(), but also calls OnComplete with the task's
    // return values once it finishes normally (status == ok) — even if
    // it wait()s/yields internally any number of times before returning.
    // Used by NetworkEvent to answer an Invoke once the handler is done.
    void SpawnWithCallback(sol::function fn,
                            std::vector<sol::object> initialArgs,
                            std::function<void(std::vector<sol::object>)> onComplete,
                            const std::string& name = "",
                            ScriptLayer layer       = ScriptLayer::Game);

    bool SpawnScript(const std::string& path,
                     double delay      = 0.0,
                     ScriptLayer layer = ScriptLayer::Game);

    bool SpawnCode(const std::string& code,
               double delay            = 0.0,
               const std::string& name = "",
               ScriptLayer layer       = ScriptLayer::Game);

    void Clear();

    int    TaskCount() const { return static_cast<int>(m_tasks.size()); }
    double Now()       const { return m_now; }

    // доступ к состоянию Lua — нужен JoinScriptLoader, чтобы выставить
    // __JoinPort__/__JoinAddress__ перед спауном host/join скрипта
    sol::state& GetLua() const { return *m_lua; }

    // Suspends the currently-resuming task (must be called from inside a
    // coroutine that's actively being ticked). Returns a fresh request id,
    // or 0 if there's no task currently running (called from the wrong place).
    // The caller is expected to follow this up with a real Lua yield
    // (e.g. via sol::yielding) so the coroutine actually stops executing.
    uint64_t SuspendCurrentTask();

    // Resumes whichever task is waiting on this requestId, delivering Args
    // as the yield's return values. Returns false if no task is waiting on
    // this id (already resumed, never existed, etc). Safe to call from
    // inside a packet-received callback during Poll(), before Tick() runs.
    bool FulfillRequest(uint64_t requestId, std::vector<sol::object> args);

private:
    KakaScheduler() = default;
    void FlushPending();

    sol::state*           m_lua            = nullptr;
    double                m_now            = 0.0;
    std::list<KakaTask>   m_tasks;
    std::list<KakaTask>   m_pending;
    bool                  m_ticking        = false;
    KakaTask*             m_currentTask    = nullptr; // set only while Tick() is resuming this task
    uint64_t              m_nextRequestId  = 1;
};