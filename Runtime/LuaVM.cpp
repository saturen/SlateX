/*
    SlateX - 2026
*/
#include "LuaVM.hpp"
#include "Engine.hpp"
#include "../Engine/HttpUtil/HttpUtil.h"
#include "../Engine/Scripting/LuaSandbox.hpp"
#include "../Engine/Scripting/KakaScheduler.hpp"
#include "../Engine/Math/Vector3.hpp"
#include "../Engine/Math/CFrame.hpp"
#include "../Engine/FemkaDM/Instance.hpp"
#include "../Engine/FemkaDM/DataModel.hpp"
#include "../Engine/Reflection/Reflection.hpp"
#include <iostream>
#include <cstdlib>

LuaVM& LuaVM::Get() {
    static LuaVM instance;
    return instance;
}

void LuaVM::RegisterMath() {
    using V3 = Vector3;

    m_lua->new_usertype<V3>("Vector3",
        sol::constructors<V3(), V3(float, float, float)>(),
        "X", &V3::X,
        "Y", &V3::Y,
        "Z", &V3::Z,
        "Magnitude",  &V3::Magnitude,
        "Normalized", &V3::Normalized,
        "Dot",        &V3::Dot,
        "Cross",      &V3::Cross,
        "Distance",   &V3::Distance,
        sol::meta_function::addition,       &V3::operator+,
        sol::meta_function::subtraction,    static_cast<V3(V3::*)(const V3&) const>(&V3::operator-),
        sol::meta_function::unary_minus,    static_cast<V3(V3::*)() const>(&V3::operator-),
        sol::meta_function::multiplication, sol::overload(
            static_cast<V3(V3::*)(const V3&) const>(&V3::operator*),
            static_cast<V3(V3::*)(float) const>(&V3::operator*)
        ),
        sol::meta_function::division, sol::overload(
            static_cast<V3(V3::*)(const V3&) const>(&V3::operator/),
            static_cast<V3(V3::*)(float) const>(&V3::operator/)
        ),
        sol::meta_function::equal_to,    &V3::operator==,
        sol::meta_function::to_string,   &V3::ToString,
        "zero",  &V3::Zero,
        "one",   &V3::One
    );

    m_lua->new_usertype<CFrame>("CFrame",
        sol::constructors<CFrame(), CFrame(const Vector3&)>(),
        "Position",           &CFrame::Position,
        "PointToWorldSpace",  &CFrame::PointToWorldSpace,
        "PointToObjectSpace", &CFrame::PointToObjectSpace,
        sol::meta_function::addition,       &CFrame::operator+,
        sol::meta_function::multiplication, &CFrame::operator*,
        sol::meta_function::equal_to,       &CFrame::operator==,
        sol::meta_function::to_string,      &CFrame::ToString,
        "identity", &CFrame::Identity
    );

    std::cout << "[LuaVM] Vector3/CFrame registered\n";
}

void LuaVM::RegisterEngineBindings() {
    // лазейка: эти два видны Lua-стороне как __Engine...__, а в
    // env (HostServer/ConnectServer) попадают только для Layer::Join
    // (см. LuaSandbox::PopulateEnv)
    (*m_lua)["__EngineHostServer__"] = [](uint16_t Port) -> bool {
        return Engine::Get().HostServer(Port);
    };

    (*m_lua)["__EngineConnectServer__"] = [](const std::string& Address, uint16_t Port) -> bool {
        return Engine::Get().ConnectToServer(Address, Port);
    };

    (*m_lua)["__game_Shutdown__"] = []() {
        Engine::Get().Shutdown();
        std::exit(0);
    };

    std::cout << "[LuaVM] Engine bindings registered\n";
}

void LuaVM::RegisterHttpBindings() {
    (*m_lua)["__HttpGet__"] = [this](const std::string& Url) -> sol::table {
        auto resp = HttpUtil::Get().Get(Url);
        sol::table t = m_lua->create_table();
        t["status"] = resp.status;
        t["body"]   = resp.body;
        t["error"]  = resp.error;
        t["ok"]     = resp.ok();
        return t;
    };

    (*m_lua)["__HttpPost__"] = [this](const std::string& Url, const std::string& Body) -> sol::table {
        auto resp = HttpUtil::Get().Post(Url, Body);
        sol::table t = m_lua->create_table();
        t["status"] = resp.status;
        t["body"]   = resp.body;
        t["error"]  = resp.error;
        t["ok"]     = resp.ok();
        return t;
    };

    std::cout << "[LuaVM] Http bindings registered\n";
}

void LuaVM::RegisterInstanceBindings() {
    // __index — сначала reflection-свойства (CFrame/Size/Anchored у BasePart
    // и т.д., см. Engine/Reflection), потом дети по имени (FindFirstChild)
    auto IndexFn = [](Instance& self, const std::string& key, sol::this_state s) -> sol::object {
        std::string className = self.GetClassName();
        while (!className.empty()) {
            const ClassDescriptor* desc = ClassRegistry::Get().Find(className);
            if (!desc) break;
            const PropertyDescriptor* prop = desc->FindProp(key);
            if (prop && prop->getLua)
                return prop->getLua(&self, s.lua_state());
            className = desc->baseClassName;
        }

        auto child = self.FindFirstChild(key);
        if (child) return sol::make_object(s.lua_state(), child);

        return sol::lua_nil;
    };

    // __newindex — то же самое, но на запись; неизвестный ключ — ошибка,
    // а не тихое создание нового поля (так и должно быть для Instance)
    auto NewIndexFn = [](Instance& self, const std::string& key, sol::object value) {
        std::string className = self.GetClassName();
        while (!className.empty()) {
            const ClassDescriptor* desc = ClassRegistry::Get().Find(className);
            if (!desc) break;
            const PropertyDescriptor* prop = desc->FindProp(key);
            if (prop && prop->setLua) {
                if (self.IsPropertyLocked(key)) {
                    std::cerr << "[LuaVM] Property '" << key << "' is locked on "
                              << self.GetClassName() << "\n";
                    return;
                }
                prop->setLua(&self, value);
                return;
            }
            className = desc->baseClassName;
        }
        std::cerr << "[LuaVM] Unknown property '" << key
                  << "' on " << self.GetClassName() << "\n";
    };

    m_lua->new_usertype<Instance>("Instance",
        sol::no_constructor,
        "Name",      sol::property(&Instance::GetName, &Instance::SetName),
        "ClassName", sol::property(&Instance::GetClassName),
        "Parent",    sol::property(
            [](Instance& self) { return self.GetParent(); },
            [](Instance& self, InstanceRef p) { self.SetParent(p); }
        ),
        "GetChildren",            &Instance::GetChildren,
        "GetDescendants",         &Instance::GetDescendants,
        "FindFirstChild",         [](Instance& self, const std::string& n) { return self.FindFirstChild(n); },
        "FindFirstChildOfClass",  &Instance::FindFirstChildOfClass,
        "FindFirstChildWhichIsA", &Instance::FindFirstChildWhichIsA,
        "FindFirstAncestor",      &Instance::FindFirstAncestor,
        "WaitForChild",           [](Instance& self, const std::string& n) { return self.WaitForChild(n); },
        "Destroy",                &Instance::Destroy,
        "Clone",                  &Instance::Clone,
        "IsA",                    &Instance::IsA,
        "GetFullName",            &Instance::GetFullName,
        "IsDescendantOf",         [](Instance& self, Instance* anc) { return self.IsDescendantOf(anc); },
        "IsAncestorOf",           [](Instance& self, Instance* desc) { return self.IsAncestorOf(desc); },
        sol::meta_function::index,     IndexFn,
        sol::meta_function::new_index, NewIndexFn
    );

    // Instance.new(ClassName) — фабрика через ClassRegistry (SLATE_CLASS_END
    // регистрирует туда конструктор каждого класса)
    sol::table InstanceTable = (*m_lua)["Instance"];
    InstanceTable.set_function("new", [](const std::string& ClassName) -> InstanceRef {
        auto inst = ClassRegistry::Get().Create(ClassName);
        if (!inst)
            std::cerr << "[LuaVM] Instance.new: unknown class '" << ClassName << "'\n";
        return inst;
    });

    (*m_lua)["game"] = DataModel::Get();
    (*m_lua)["Game"] = DataModel::Get();

    std::cout << "[LuaVM] Instance/DataModel registered\n";
}

void LuaVM::Init() {
    if (m_initialized) return;
    m_initialized = true;

    m_lua = std::make_unique<sol::state>();
    m_lua->open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::string,
        sol::lib::table, sol::lib::coroutine, sol::lib::os
    );

    RegisterMath();
    RegisterInstanceBindings();
    RegisterEngineBindings();
    RegisterHttpBindings();

    // один раз на процесс — реальная изоляция по слоям происходит
    // через LuaSandbox::CreateEnv(layer) на каждый отдельный скрипт
    LuaSandbox::Apply(*m_lua, ScriptLayer::Join);
    KakaScheduler::Get().Init(*m_lua);

    std::cout << "[LuaVM] Initialized\n";
}