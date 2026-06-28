/*
    SlateX - 2026
*/
#include "KakaScheduler.hpp"
#include "LuaSandbox.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

KakaScheduler& KakaScheduler::Get() {
    static KakaScheduler instance;
    return instance;
}

void KakaScheduler::Init(sol::state& lua) {
    m_lua = &lua;

    lua["wait"] = sol::yielding([](double n) -> double {
        return (n < 0.0) ? 0.0 : n;
    });

    lua["spawn"] = [this](sol::function fn) {
        Spawn(fn, 0.0, "spawned", ScriptLayer::Game);
    };

    lua["delay"] = [this](double n, sol::function fn) {
        Spawn(fn, n, "delayed", ScriptLayer::Game);
    };

    std::cout << "[KakaScheduler] Initialized\n";
}

void KakaScheduler::Tick(double now) {
    if (!m_lua) return;
    m_now     = now;
    m_ticking = true;

    for (auto& task : m_tasks) {
        std::vector<sol::object> resumeArgs;

        if (task.waitingForRequest) {
            // Resumed only by FulfillRequest(), never by time.
            if (!task.responseReady) continue;
            resumeArgs = std::move(task.pendingResponseArgs);
            task.waitingForRequest = false;
            task.responseReady     = false;
        } else {
            if (task.resumeAt > now) continue;
        }

        if (!task.co.runnable()) continue;

        m_currentTask = &task;
        auto result = resumeArgs.empty()
            ? task.co()
            : task.co(sol::as_args(resumeArgs));
        m_currentTask = nullptr;

        if (result.status() == sol::call_status::yielded) {
            if (task.waitingForRequest) {
                // SuspendCurrentTask() was already called from inside this
                // resume — task.requestId is set, nothing more to do here.
            } else {
                double waitSec = 0.0;
                auto val = result.get<sol::optional<double>>(0);
                if (val) waitSec = *val;
                task.resumeAt = now + waitSec;
            }
        } else if (result.status() == sol::call_status::ok) {
            if (task.onComplete) {
                std::vector<sol::object> outArgs;
                for (int i = 0; i < static_cast<int>(result.return_count()); i++)
                    outArgs.push_back(result.get<sol::object>(i));
                task.onComplete(outArgs);
                task.onComplete = nullptr;
            }
        } else {
            sol::error err = result;
            std::cerr << "[KakaScheduler] Error in '"
                      << task.name << "' (layer "
                      << static_cast<int>(task.layer) << "): "
                      << err.what() << "\n";
        }
    }

    // std::list::remove_if не двигает соседние элементы при удалении —
    // в отличие от vector+remove_if+erase, которое ломает уже стартовавшие
    // (yielded) sol::coroutine при компакции (см. фикс от 2026-06-21)
    m_tasks.remove_if(
        [](const KakaTask& t) { return !t.co.runnable(); }
    );

    // fire due timers — plain C++ callbacks, separate from da coroutine
    // task list above, see CallLater
    for (auto it = m_timers.begin(); it != m_timers.end(); ) {
        if (now >= it->first) {
            auto fn = std::move(it->second);
            it = m_timers.erase(it);
            fn();
        } else {
            ++it;
        }
    }

    m_ticking = false;
    FlushPending();
}

uint64_t KakaScheduler::SuspendCurrentTask() {
    if (!m_currentTask) {
        std::cerr << "[KakaScheduler] SuspendCurrentTask called with no active task\n";
        return 0;
    }

    uint64_t id = m_nextRequestId++;
    m_currentTask->waitingForRequest = true;
    m_currentTask->requestId         = id;
    return id;
}

bool KakaScheduler::FulfillRequest(uint64_t requestId, std::vector<sol::object> args) {
    for (auto& task : m_tasks) {
        if (task.waitingForRequest && task.requestId == requestId) {
            task.pendingResponseArgs = std::move(args);
            task.responseReady       = true;
            return true;
        }
    }
    // also check m_pending, in case a request resolves before FlushPending runs
    for (auto& task : m_pending) {
        if (task.waitingForRequest && task.requestId == requestId) {
            task.pendingResponseArgs = std::move(args);
            task.responseReady       = true;
            return true;
        }
    }
    return false;
}

void KakaScheduler::CallLater(double DelaySec, std::function<void()> Fn) {
    m_timers.push_back({ m_now + DelaySec, std::move(Fn) });
}

void KakaScheduler::Spawn(sol::function fn, double delay,
                           const std::string& name, ScriptLayer layer) {
    if (!m_lua) return;

    sol::thread thread = sol::thread::create(m_lua->lua_state());
    sol::coroutine co(thread.state(), fn);

    KakaTask task {
        std::move(thread),
        std::move(co),
        m_now + delay,
        name.empty() ? "task" : name,
        layer
    };

    if (m_ticking)
        m_pending.push_back(std::move(task));
    else
        m_tasks.push_back(std::move(task));
}

void KakaScheduler::SpawnWithArgs(sol::function fn, std::vector<sol::object> initialArgs,
                                    const std::string& name, ScriptLayer layer) {
    if (!m_lua) return;

    sol::thread thread = sol::thread::create(m_lua->lua_state());
    sol::coroutine co(thread.state(), fn);

    KakaTask task {
        std::move(thread),
        std::move(co),
        m_now, // resumeAt is irrelevant here — the request-style path below is used instead
        name.empty() ? "task" : name,
        layer
    };

    // pre-fulfilled "request" — the very first resume in Tick() will deliver
    // initialArgs as this coroutine's normal call arguments, same as how
    // a fresh coroutine.resume(co, ...) passes args to the function body
    task.waitingForRequest    = true;
    task.responseReady        = true;
    task.pendingResponseArgs  = std::move(initialArgs);

    if (m_ticking)
        m_pending.push_back(std::move(task));
    else
        m_tasks.push_back(std::move(task));
}

void KakaScheduler::SpawnWithCallback(sol::function fn, std::vector<sol::object> initialArgs,
                                        std::function<void(std::vector<sol::object>)> onComplete,
                                        const std::string& name, ScriptLayer layer) {
    if (!m_lua) return;

    sol::thread thread = sol::thread::create(m_lua->lua_state());
    sol::coroutine co(thread.state(), fn);

    KakaTask task {
        std::move(thread),
        std::move(co),
        m_now,
        name.empty() ? "task" : name,
        layer
    };

    task.waitingForRequest   = true;
    task.responseReady       = true;
    task.pendingResponseArgs = std::move(initialArgs);
    task.onComplete          = std::move(onComplete);

    if (m_ticking)
        m_pending.push_back(std::move(task));
    else
        m_tasks.push_back(std::move(task));
}

bool KakaScheduler::SpawnScript(const std::string& path,
                                 double delay, ScriptLayer layer) {
    if (!m_lua) return false;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[KakaScheduler] Cannot open script: " << path << "\n";
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    const std::string src = ss.str();

    auto loadResult = m_lua->load(src, path, sol::load_mode::text);
    if (!loadResult.valid()) {
        sol::error err = loadResult;
        std::cerr << "[KakaScheduler] Load error in '"
                  << path << "': " << err.what() << "\n";
        return false;
    }

    sol::function fn = loadResult;

    sol::environment env(*m_lua, sol::create, m_lua->globals());
    sol::table sandboxEnv = LuaSandbox::CreateEnv(*m_lua, layer);
    for (auto& kv : sandboxEnv)
        env.set(kv.first, kv.second);
    env["_G"] = env;

    env.set_on(fn);

    Spawn(fn, delay, path, layer);
    std::cout << "[KakaScheduler] Spawned script: " << path
              << " (layer " << static_cast<int>(layer) << ")\n";
    return true;
}

bool KakaScheduler::SpawnCode(const std::string& code,
                               double delay,
                               const std::string& name,
                               ScriptLayer layer) {
    if (!m_lua) return false;

    auto loadResult = m_lua->load(code, name, sol::load_mode::text);
    if (!loadResult.valid()) {
        sol::error err = loadResult;
        std::cerr << "[KakaScheduler] Load error in '"
                  << name << "': " << err.what() << "\n";
        return false;
    }

    sol::function fn = loadResult;
    sol::environment env(*m_lua, sol::create, m_lua->globals());
    sol::table sandboxEnv = LuaSandbox::CreateEnv(*m_lua, layer);
    for (auto& kv : sandboxEnv)
        env.set(kv.first, kv.second);
    env["_G"] = env;
    env.set_on(fn);

    Spawn(fn, delay, name, layer);
    std::cout << "[KakaScheduler] Spawned code: " << name
              << " (layer " << static_cast<int>(layer) << ")\n";
    return true;
}

void KakaScheduler::Clear() {
    m_tasks.clear();
    m_pending.clear();
    std::cout << "[KakaScheduler] Cleared\n";
}

void KakaScheduler::FlushPending() {
    if (m_pending.empty()) return;
    for (auto& t : m_pending)
        m_tasks.push_back(std::move(t));
    m_pending.clear();
}