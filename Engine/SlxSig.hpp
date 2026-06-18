/*
    SlateX - 2026
*/
#pragma once
#include <functional>
#include <vector>
#include <memory>
#include <algorithm>
#include <tuple>

// forward declare — signal needs to spawn through scheduler
// but doesnt directly depend on it
class KakaScheduler;

// connection handle — keep this alive to keep the connection alive
// let it die and connection disconnects automatically
struct SignalConnection {
    bool Connected = true;
    void Disconnect() { Connected = false; }
};
using ConnectionRef = std::shared_ptr<SignalConnection>;

// SlxSig — the right way to do events in SlateX
//
// key insight stolen from C# version:
// Fire() does NOT call handlers directly
// it spawns them through KakaScheduler
// this way handlers can yield, and we dont blow up the lua stack
//
// Connect   — persistent, fires every time
// Once      — fires once then removes itself
// DestroyAll — nukes all connections

template<typename... Args>
class SlxSig {
public:
    using Handler = std::function<void(Args...)>;

    // set the scheduler — call this once at init
    // if null, fires directly (da fallback, not ideal)
    static void SetScheduler(KakaScheduler* Scheduler) {
        s_scheduler = Scheduler;
    }

    // returns a connection ref — hold onto it or it disconnects
    ConnectionRef Connect(Handler Fn) {
        auto Conn = std::make_shared<SignalConnection>();
        m_persistent.push_back({ Fn, Conn });
        return Conn;
    }

    // fires once then auto-disconnects
    ConnectionRef Once(Handler Fn) {
        auto Conn = std::make_shared<SignalConnection>();
        m_once.push_back({ Fn, Conn });
        return Conn;
    }

    // fire — spawns all handlers through scheduler
    // args are copied into the lambda capture
    void Fire(Args... args) {
        // persistent handlers
        auto Snapshot = m_persistent;
        for (auto& Entry : Snapshot) {
            if (!Entry.Conn || !Entry.Conn->Connected) continue;
            SpawnHandler(Entry.Fn, args...);
        }

        // cleanup dead connections
        m_persistent.erase(
            std::remove_if(m_persistent.begin(), m_persistent.end(),
                [](const Entry& E) { return !E.Conn || !E.Conn->Connected; }),
            m_persistent.end()
        );

        // once handlers — reset before calling so they dont double fire
        auto Once = std::move(m_once);
        m_once.clear();
        for (auto& Entry : Once) {
            if (!Entry.Conn || !Entry.Conn->Connected) continue;
            Entry.Conn->Connected = false;
            SpawnHandler(Entry.Fn, args...);
        }
    }

    // nuke everything
    void DestroyAll() {
        for (auto& E : m_persistent)
            if (E.Conn) E.Conn->Connected = false;
        for (auto& E : m_once)
            if (E.Conn) E.Conn->Connected = false;
        m_persistent.clear();
        m_once.clear();
    }

    int ConnectionCount() const {
        int Count = 0;
        for (const auto& E : m_persistent)
            if (E.Conn && E.Conn->Connected) Count++;
        return Count;
    }

private:
    struct Entry {
        Handler       Fn;
        ConnectionRef Conn;
    };

    std::vector<Entry> m_persistent;
    std::vector<Entry> m_once;

    static KakaScheduler* s_scheduler;

    void SpawnHandler(Handler& Fn, Args... args) {
        if (s_scheduler) {
            auto Captured     = Fn;
            auto CapturedArgs = std::make_tuple(args...);
            ScheduleOnScheduler([Captured, CapturedArgs]() mutable {
                std::apply(Captured, CapturedArgs);
            });
        } else {
            // da fallback — direct call, no yielding possible
            Fn(args...);
        }
    }

    // implemented in SlxSig.cpp to break circular include with KakaScheduler
    static void ScheduleOnScheduler(std::function<void()> Task);
};

template<typename... Args>
KakaScheduler* SlxSig<Args...>::s_scheduler = nullptr;