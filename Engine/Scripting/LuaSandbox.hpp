/*
    SlateX - 2026
*/
#pragma once
#include <sol/sol.hpp>

// LuaSandbox — изоляция Lua окружения
//
//   Layer::Join  (0) — джойн/хост скрипты, полный доступ + HostServer/ConnectServer
//   Layer::Core  (1) — корскрипты, без доступа к Join-биндингам
//   Layer::Game  (2) — обычные скрипты/модули, только явный whitelist
//
// Применяется через Apply(lua, layer) один раз
// после LuaVM::Init() и регистрации биндингов.

#ifdef _WIN32
#define ENGINE_API __declspec(dllexport)
#else
#define ENGINE_API
#endif

enum class ScriptLayer {
    Join = 0,  // максимальные права
    Core = 1,  // средние права
    Game = 2,  // минимальные права
};

class ENGINE_API LuaSandbox {
public:
    // применить sandbox к состоянию Lua (вызывать после регистрации всех биндингов)
    static void Apply(sol::state& lua, ScriptLayer layer);

    // создать изолированное окружение для одного скрипта
    static sol::table CreateEnv(sol::state& lua, ScriptLayer layer);

private:
    static void StripDangerous(sol::state& lua);
    static void SanitizeOs(sol::state& lua);
    static void PopulateEnv(sol::state& lua, sol::table& env, ScriptLayer layer);
};