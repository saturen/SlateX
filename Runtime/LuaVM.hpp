/*
    SlateX - 2026
*/
#pragma once
#include <sol/sol.hpp>
#include <memory>

// LuaVM — владеет sol::state, регистрирует usertype'ы и биндинги,
// дёргает LuaSandbox::Apply + KakaScheduler::Init.
//
// Живёт в Runtime, а не в Engine/Scripting — потому что биндинги
// HostServer/ConnectServer должны видеть класс Engine (фасад над
// ServerReplicator/ClientReplicator), а Engine-библиотека про
// Runtime/Network ничего не знает (см. Engine.hpp — та же причина,
// по которой Engine.cpp переехал сюда).
class LuaVM {
public:
    static LuaVM& Get();

    // создаёт sol::state, регистрирует Vector3/CFrame, HostServer/
    // ConnectServer/HttpGet/HttpPost/Shutdown, вызывает LuaSandbox::Apply
    // и KakaScheduler::Init — вызывать один раз при старте процесса
    void Init();

    sol::state& Lua() { return *m_lua; }

private:
    LuaVM() = default;

    void RegisterMath();
    void RegisterEngineBindings();
    void RegisterHttpBindings();
    void RegisterInstanceBindings();
    void RegisterSignalBindings();
    void RegisterPlayerBindings();

    std::unique_ptr<sol::state> m_lua;
    bool m_initialized = false;
};