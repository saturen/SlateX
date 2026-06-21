/*
    SlateX - 2026
*/
#pragma once
#include <sol/sol.hpp>
#include <vector>
#include "../Transport/Serializer.hpp"

// LuaArgSerializer — рекурсивная сериализация Lua-значений в байты
// для NetworkEvent (FireServer/FireClient/InvokeServer/InvokeClient).
//
// Формат каждого значения: [1 байт тег][payload]
// Table сериализуется как список (ключ,значение) пар, оба элемента
// пары — тоже рекурсивные LuaArg значения (так одним форматом
// покрываются и массивы, и dict-таблицы).

enum class LuaArgTag : uint8_t {
    Nil         = 0,
    Bool        = 1,
    Number      = 2,
    String      = 3,
    Vector3     = 4,
    CFrame      = 5,
    InstanceRef = 6,
    Table       = 7,
};

class LuaArgSerializer {
public:
    static void       SerializeArg(Serializer& S, const sol::object& Val);
    static sol::object DeserializeArg(Deserializer& D, sol::state_view Lua);

    // целый список аргументов одного вызова (FireServer(a, b, c) и т.д.)
    static void SerializeArgs(Serializer& S, const std::vector<sol::object>& Args);
    static std::vector<sol::object> DeserializeArgs(Deserializer& D, sol::state_view Lua);
};