/*
    SlateX - 2026
*/
#include "LuaArgSerializer.hpp"
#include "../../Engine/FemkaDM/Instance.hpp"
#include "../../Engine/Math/Vector3.hpp"
#include "../../Engine/Math/CFrame.hpp"
#include <iostream>

void LuaArgSerializer::SerializeArg(Serializer& S, const sol::object& Val) {
    if (!Val.valid() || Val.get_type() == sol::type::nil) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::Nil));
        return;
    }

    if (Val.is<bool>() && Val.get_type() == sol::type::boolean) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::Bool));
        S.WriteBool(Val.as<bool>());
        return;
    }

    if (Val.is<InstanceRef>()) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::InstanceRef));
        auto inst = Val.as<InstanceRef>();
        S.WriteUInt32(inst ? inst->GetNetId() : 0);
        return;
    }

    if (Val.is<Vector3>()) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::Vector3));
        auto v = Val.as<Vector3>();
        S.WriteFloat(v.X); S.WriteFloat(v.Y); S.WriteFloat(v.Z);
        return;
    }

    if (Val.is<CFrame>()) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::CFrame));
        auto c = Val.as<CFrame>();
        S.WriteFloat(c.Position.X); S.WriteFloat(c.Position.Y); S.WriteFloat(c.Position.Z);
        for (int i = 0; i < 9; i++) S.WriteFloat(c.R[i]);
        return;
    }

    if (Val.get_type() == sol::type::number) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::Number));
        S.WriteDouble(Val.as<double>());
        return;
    }

    if (Val.get_type() == sol::type::string) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::String));
        S.WriteString(Val.as<std::string>());
        return;
    }

    if (Val.get_type() == sol::type::table) {
        S.WriteByte(static_cast<uint8_t>(LuaArgTag::Table));
        sol::table t = Val.as<sol::table>();

        uint32_t count = 0;
        for (auto& kv : t) (void)kv, count++;
        S.WriteUInt32(count);

        for (auto& kv : t) {
            SerializeArg(S, kv.first);
            SerializeArg(S, kv.second);
        }
        return;
    }

    // функции/userdata неизвестных типов — не сериализуемы, шлём nil
    std::cerr << "[LuaArgSerializer] Cannot serialize value of type "
              << static_cast<int>(Val.get_type()) << ", sending nil\n";
    S.WriteByte(static_cast<uint8_t>(LuaArgTag::Nil));
}

sol::object LuaArgSerializer::DeserializeArg(Deserializer& D, sol::state_view Lua) {
    auto tag = static_cast<LuaArgTag>(D.ReadByte());

    switch (tag) {
        case LuaArgTag::Nil:
            return sol::make_object(Lua, sol::lua_nil);

        case LuaArgTag::Bool:
            return sol::make_object(Lua, D.ReadBool());

        case LuaArgTag::Number:
            return sol::make_object(Lua, D.ReadDouble());

        case LuaArgTag::String:
            return sol::make_object(Lua, D.ReadString());

        case LuaArgTag::Vector3: {
            float x = D.ReadFloat(), y = D.ReadFloat(), z = D.ReadFloat();
            return sol::make_object(Lua, Vector3(x, y, z));
        }

        case LuaArgTag::CFrame: {
            float pos[3];
            for (int i = 0; i < 3; i++) pos[i] = D.ReadFloat();
            float rot[9];
            for (int i = 0; i < 9; i++) rot[i] = D.ReadFloat();
            return sol::make_object(Lua, CFrame(Vector3(pos[0], pos[1], pos[2]), rot));
        }

        case LuaArgTag::InstanceRef: {
            uint32_t netId = D.ReadUInt32();
            auto inst = Instance::FindByNetId(netId);
            if (!inst) return sol::make_object(Lua, sol::lua_nil);
            return sol::make_object(Lua, inst);
        }

        case LuaArgTag::Table: {
            sol::table t = Lua.create_table();
            uint32_t count = D.ReadUInt32();
            for (uint32_t i = 0; i < count; i++) {
                sol::object key = DeserializeArg(D, Lua);
                sol::object val = DeserializeArg(D, Lua);
                t.set(key, val);
            }
            return t;
        }
    }

    return sol::make_object(Lua, sol::lua_nil);
}

void LuaArgSerializer::SerializeArgs(Serializer& S, const std::vector<sol::object>& Args) {
    S.WriteUInt32(static_cast<uint32_t>(Args.size()));
    for (auto& a : Args)
        SerializeArg(S, a);
}

std::vector<sol::object> LuaArgSerializer::DeserializeArgs(Deserializer& D, sol::state_view Lua) {
    std::vector<sol::object> out;
    uint32_t count = D.ReadUInt32();
    out.reserve(count);
    for (uint32_t i = 0; i < count; i++)
        out.push_back(DeserializeArg(D, Lua));
    return out;
}