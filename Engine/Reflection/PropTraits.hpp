/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <optional>
#include <cstring>
#include <sol/sol.hpp>
#include "../FemkaDM/Instance.hpp"
#include "../Math/Vector3.hpp"
#include "../Math/CFrame.hpp"
#include "Reflection.hpp"

// PropTraits<T>.
// Reflection.hpp::MakeProperty<> uses those 4 properties
// to avoid repeating getLua/setLua/serialize/deserialize every time
//
//   kType      — PropType for networking
//   ToLua       — C++ -> sol::object
//   FromLua     — sol::object -> std::optional<T> (nullopt if something wrong)
//   Serialize   — T -> raw bytes (std::string as container)
//   Deserialize — raw bytes -> T

template<typename T>
struct PropTraits;

// --- bool ---
template<>
struct PropTraits<bool> {
    static constexpr PropType kType = PropType::Bool;

    static sol::object ToLua(bool v, lua_State* L) {
        return sol::make_object(L, v);
    }
    static std::optional<bool> FromLua(sol::object v) {
        if (!v.is<bool>()) return std::nullopt;
        return v.as<bool>();
    }
    static std::string Serialize(bool v) {
        return std::string(1, static_cast<char>(v ? 1 : 0));
    }
    static bool Deserialize(const std::string& data) {
        return !data.empty() && data[0] != 0;
    }
};

// --- float ---
template<>
struct PropTraits<float> {
    static constexpr PropType kType = PropType::Float;

    static sol::object ToLua(float v, lua_State* L) {
        return sol::make_object(L, v);
    }
    static std::optional<float> FromLua(sol::object v) {
        if (!v.is<float>()) return std::nullopt;
        return v.as<float>();
    }
    static std::string Serialize(float v) {
        std::string out(sizeof(float), '\0');
        std::memcpy(out.data(), &v, sizeof(float));
        return out;
    }
    static float Deserialize(const std::string& data) {
        float v = 0.f;
        if (data.size() >= sizeof(float))
            std::memcpy(&v, data.data(), sizeof(float));
        return v;
    }
};

// --- Vector3 ---
template<>
struct PropTraits<Vector3> {
    static constexpr PropType kType = PropType::Vector3;

    static sol::object ToLua(Vector3 v, lua_State* L) {
        return sol::make_object(L, v);
    }
    static std::optional<Vector3> FromLua(sol::object v) {
        if (!v.is<Vector3>()) return std::nullopt;
        return v.as<Vector3>();
    }
    static std::string Serialize(Vector3 v) {
        std::string out(sizeof(float) * 3, '\0');
        std::memcpy(&out[0],              &v.X, sizeof(float));
        std::memcpy(&out[sizeof(float)],  &v.Y, sizeof(float));
        std::memcpy(&out[sizeof(float)*2],&v.Z, sizeof(float));
        return out;
    }
    static Vector3 Deserialize(const std::string& data) {
        Vector3 v;
        if (data.size() < sizeof(float) * 3) return v;
        std::memcpy(&v.X, &data[0],               sizeof(float));
        std::memcpy(&v.Y, &data[sizeof(float)],   sizeof(float));
        std::memcpy(&v.Z, &data[sizeof(float)*2], sizeof(float));
        return v;
    }
};

// --- CFrame ---
template<>
struct PropTraits<CFrame> {
    static constexpr PropType kType = PropType::CFrame;

    static sol::object ToLua(CFrame v, lua_State* L) {
        return sol::make_object(L, v);
    }
    static std::optional<CFrame> FromLua(sol::object v) {
        if (!v.is<CFrame>()) return std::nullopt;
        return v.as<CFrame>();
    }
    // позиция (3 float) + 9 float матрицы поворота = 48 байт
    static std::string Serialize(CFrame v) {
        std::string out(sizeof(float) * 12, '\0');
        float* dst = reinterpret_cast<float*>(out.data());
        dst[0] = v.Position.X; dst[1] = v.Position.Y; dst[2] = v.Position.Z;
        for (int i = 0; i < 9; i++) dst[3 + i] = v.R[i];
        return out;
    }
    static CFrame Deserialize(const std::string& data) {
        CFrame v;
        if (data.size() < sizeof(float) * 12) return v;
        const float* src = reinterpret_cast<const float*>(data.data());
        v.Position = Vector3(src[0], src[1], src[2]);
        for (int i = 0; i < 9; i++) v.R[i] = src[3 + i];
        return v;
    }
};

// --- std::string ---
template<>
struct PropTraits<std::string> {
    static constexpr PropType kType = PropType::String;

    static sol::object ToLua(const std::string& v, lua_State* L) {
        return sol::make_object(L, v);
    }
    static std::optional<std::string> FromLua(sol::object v) {
        if (!v.is<std::string>()) return std::nullopt;
        return v.as<std::string>();
    }
    static std::string Serialize(const std::string& v) { return v; }
    static std::string Deserialize(const std::string& data) { return data; }
};