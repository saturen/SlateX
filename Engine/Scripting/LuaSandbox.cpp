/*
    SlateX - 2026
    LuaSandbox — реализация
*/
#include "LuaSandbox.hpp"
#include <iostream>

void LuaSandbox::StripDangerous(sol::state& lua) {
    const char* remove[] = {
        "io", "debug", "require", "dofile",
        "loadfile", "load", "loadstring",
        "collectgarbage",
    };
    for (auto* name : remove)
        lua[name] = sol::nil;
    std::cout << "[LuaSandbox] Dangerous globals stripped\n";
}

void LuaSandbox::SanitizeOs(sol::state& lua) {
    sol::table oldOs = lua["os"];
    if (!oldOs.valid()) return;

    sol::table safeOs = lua.create_table();
    safeOs["time"]     = oldOs["time"];
    safeOs["clock"]    = oldOs["clock"];
    safeOs["difftime"] = oldOs["difftime"];
    safeOs["date"]     = oldOs["date"];

    lua["os"] = safeOs;
    std::cout << "[LuaSandbox] os sanitized\n";
}

void LuaSandbox::PopulateEnv(sol::state& lua, sol::table& env, ScriptLayer layer) {
    const char* base[] = {
        "math", "string", "table", "coroutine",
        "pairs", "ipairs", "next",
        "tostring", "tonumber", "type",
        "pcall", "xpcall", "error",
        "select", "rawget", "rawset", "rawequal",
        "setmetatable", "getmetatable",
        "print", "os",
        "game", "Game", "Instance",
        "wait", "spawn", "delay",
        "Vector3", "CFrame",
    };
    for (auto* name : base) {
        sol::object val = lua[name];
        if (val.valid())
            env[name] = val;
        else
            std::cerr << "[LuaSandbox] MISSING global: " << name << "\n";
    }

    if (layer == ScriptLayer::Core || layer == ScriptLayer::Join) {
        sol::object warn = lua["warn"];
        if (warn.valid()) env["warn"] = warn;
    }

    if (layer == ScriptLayer::Join) {
        // HostServer/ConnectServer — лазейка: джойн-скрипт может
        // дёрнуть HostServer() и сам решить стать хостом, даже если
        // его загрузили как join-скрипт. см. Engine::HostServer/ConnectToServer
        sol::object hostFn    = lua["__EngineHostServer__"];
        sol::object connectFn = lua["__EngineConnectServer__"];
        if (hostFn.valid())    env["HostServer"]    = hostFn;
        if (connectFn.valid()) env["ConnectServer"] = connectFn;

        // Port/Address — то, что пришло из CLI (-h/-j), скрипт сам
        // решает, что с этим делать
        sol::object joinPort    = lua["__JoinPort__"];
        sol::object joinAddress = lua["__JoinAddress__"];
        if (joinPort.valid())    env["Port"]    = joinPort;
        if (joinAddress.valid()) env["Address"] = joinAddress;

        // HTTP
        sol::object httpGet  = lua["__HttpGet__"];
        sol::object httpPost = lua["__HttpPost__"];
        if (httpGet.valid())  env["HttpGet"]  = httpGet;
        if (httpPost.valid()) env["HttpPost"] = httpPost;

        // Shutdown
        sol::object shutdown = lua["__game_Shutdown__"];
        if (shutdown.valid()) env["Shutdown"] = shutdown;
    }

    env["_G"] = env;
}

void LuaSandbox::Apply(sol::state& lua, ScriptLayer layer) {
    StripDangerous(lua);
    SanitizeOs(lua);
    std::cout << "[LuaSandbox] Applied for layer "
              << static_cast<int>(layer) << "\n";
}

sol::table LuaSandbox::CreateEnv(sol::state& lua, ScriptLayer layer) {
    sol::table env = lua.create_table();
    PopulateEnv(lua, env, layer);
    return env;
}