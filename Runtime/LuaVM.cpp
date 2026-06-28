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
#include "../Network/Shared/NetworkEvent.hpp"
#include "../Network/Shared/Signal.hpp"
#include "../Network/Shared/Players.hpp"
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
    m_lua->new_usertype<Instance>("Instance",
        sol::no_constructor,
        "Name",      sol::property(&Instance::GetName, &Instance::SetName),
        "ClassName", sol::property(&Instance::GetClassName),
        "Parent",    sol::property(
            [](Instance& self, sol::this_state s) -> sol::object {
                return PushInstance(self.GetParent(), s.lua_state());
            },
            [](Instance& self, InstanceRef p) { self.SetParent(p); }
        ),
        "GetChildren", [](Instance& self, sol::this_state s) {
            std::vector<sol::object> out;
            for (auto& c : self.GetChildren())
                out.push_back(PushInstance(c, s.lua_state()));
            return out;
        },
        "GetDescendants", [](Instance& self, sol::this_state s) {
            std::vector<sol::object> out;
            for (auto& c : self.GetDescendants())
                out.push_back(PushInstance(c, s.lua_state()));
            return out;
        },
        "FindFirstChild", [](Instance& self, const std::string& n, sol::this_state s) {
            return PushInstance(self.FindFirstChild(n), s.lua_state());
        },
        "FindFirstChildOfClass", [](Instance& self, const std::string& cn, sol::this_state s) {
            return PushInstance(self.FindFirstChildOfClass(cn), s.lua_state());
        },
        "FindFirstChildWhichIsA", [](Instance& self, const std::string& cn, sol::this_state s) {
            return PushInstance(self.FindFirstChildWhichIsA(cn), s.lua_state());
        },
        "FindFirstAncestor", [](Instance& self, const std::string& n, sol::this_state s) {
            return PushInstance(self.FindFirstAncestor(n), s.lua_state());
        },
        "Destroy", &Instance::Destroy,
        "Clone", [](Instance& self, sol::this_state s) {
            return PushInstance(self.Clone(), s.lua_state());
        },
        "IsA",                    &Instance::IsA,
        "GetFullName",            &Instance::GetFullName,
        "IsDescendantOf",         [](Instance& self, Instance* anc) { return self.IsDescendantOf(anc); },
        "IsAncestorOf",           [](Instance& self, Instance* desc) { return self.IsAncestorOf(desc); },
        sol::meta_function::index,     InstanceIndex,
        sol::meta_function::new_index, InstanceNewIndex
    );

    // Instance.new(ClassName) — фабрика через ClassRegistry (SLATE_CLASS_END
    // регистрирует туда конструктор каждого класса).
    //
    // ВАЖНО: возвращаем через ClassRegistry::Push, а не голый InstanceRef —
    // sol2 типизирует объект по статическому C++ типу значения, которое ему
    // передали, а не по динамическому через RTTI. Если бы биндинг ниже был
    // объявлен как "-> InstanceRef", любой класс со своими native sol2-
    // членами (как NetworkEvent.Type/.Fired/:Fire()) был бы не виден из Lua —
    // см. комментарий у PushFn в Reflection.hpp.
    sol::table InstanceTable = (*m_lua)["Instance"];
    InstanceTable.set_function("new", [](const std::string& ClassName, sol::this_state s) -> sol::object {
        auto inst = ClassRegistry::Get().Create(ClassName);
        if (!inst) {
            std::cerr << "[LuaVM] Instance.new: unknown class '" << ClassName << "'\n";
            return sol::lua_nil;
        }
        return ClassRegistry::Get().Push(inst, s.lua_state());
    });

    (*m_lua)["game"] = DataModel::Get();
    (*m_lua)["Game"] = DataModel::Get();

    // __WaitForChildYield — internal only, never called directly from
    // scripts. Suspends da calling task, registers it as a waiter on Name,
    // and (if TimeoutSec > 0) races a CallLater timeout against it —
    // whichever resolves first wins, da second FulfillRequest is a no-op.
    // Returns nothing itself — da real value (found Instance, or nil on
    // timeout) arrives later as da resumeArgs when something fulfills it.
    sol::table InstanceTable2 = (*m_lua)["Instance"];
    InstanceTable2.set_function("__WaitForChildYield", sol::yielding(
        [](Instance& self, const std::string& Name, double TimeoutSec) {
            uint64_t Id = KakaScheduler::Get().SuspendCurrentTask();
            self.RegisterChildWaiter(Name, Id);

            if (TimeoutSec > 0.0) {
                KakaScheduler::Get().CallLater(TimeoutSec, [Id]() {
                    KakaScheduler::Get().FulfillRequest(Id, {});
                });
            }
        }
    ));

    // Instance:WaitForChild(name, timeout) — da actual fast/slow branching.
    // Has to be Lua-side: calling a sol::yielding function from INSIDE
    // another C++ function doesnt reliably yield across da native call
    // boundary, da yield needs a direct Lua call site, same as Signal:Wait()
    m_lua->script(R"LUACODE(
        function Instance:WaitForChild(name, timeout)
            local existing = self:FindFirstChild(name)
            if existing then return existing end
            return self:__WaitForChildYield(name, timeout or 0)
        end
    )LUACODE");

    std::cout << "[LuaVM] Instance/DataModel registered\n";
}

void LuaVM::RegisterSignalBindings() {
    // Signal backs NetworkEvent's Fired/OnServer/OnClient — see Signal.hpp.
    // SetConnection/DoOnce just register a callback and return immediately
    // (no yield needed); Wait() is the one that actually suspends the
    // calling task, so it's the only method wrapped in sol::yielding.
    m_lua->new_usertype<Signal>("Signal",
        sol::no_constructor,
        "SetConnection",    &Signal::SetConnection,
        "DoOnce",            &Signal::DoOnce,
        "RemoveConnection",  &Signal::RemoveConnection,
        "Wait",               sol::yielding(&Signal::Wait),
        "Fire", [](Signal& self, sol::variadic_args va) {
            self.Fire(std::vector<sol::object>(va.begin(), va.end()));
        }
    );

    std::cout << "[LuaVM] Signal registered\n";
}

void LuaVM::RegisterPlayerBindings() {
    // Player's actual usertype (no_constructor + bases<Instance> + reattached
    // index/new_index) gets auto-generated by SLATE_CLASS_END now (see
    // Player.cpp) — all dis does is bolt a custom to_string onto da SAME
    // table afterward, since da macro has no slot for "one extra meta_
    // function" and Player doesnt have enough native members of its own to
    // justify a fully custom BindLua like NetworkEvent/Players have.
    //
    // Without dis, tostring(player) used to print "sol.sol::d::u<Player>:
    // 0x..." (sol2's auto-fallback for a type with no usertype at all,
    // back before Player had ANY registration) — now it's just da macro's
    // plain default (no custom to_string), which sol2 normally renders as
    // something like "Player: 0x...". Either way, nicer than dat.
    sol::table PlayerTable = (*m_lua)["Player"];
    PlayerTable[sol::meta_function::to_string] = [](Player& self) {
        return "Player(\"" + self.GetName() + "\")";
    };

    std::cout << "[LuaVM] Player to_string registered\n";
}

void LuaVM::Init() {
    if (m_initialized) return;
    m_initialized = true;

    m_lua = std::make_unique<sol::state>();
    m_lua->open_libraries(
        sol::lib::base, sol::lib::math, sol::lib::string,
        sol::lib::table, sol::lib::coroutine, sol::lib::os
    );

    // explicit call, not a static-initializer trick — see da comment on
    // Players::RegisterClass for why dat matters. Has to happen before
    // RegisterInstanceBindings, since dat's what first calls
    // DataModel::Get() -> CreateDefaultServices, which needs da "Players"
    // factory to already exist by then
    Players::RegisterClass();

    RegisterMath();
    RegisterInstanceBindings();
    RegisterSignalBindings();

    // binds EVERY registered class (NetworkEvent, Players, Player,
    // BasePart, PVInstance, anything future) in one go, base classes
    // before derived — see ClassRegistry::BindAllToLua. Used to be a
    // manual new_usertype<T> call per class right here in LuaVM.cpp,
    // which is exactly how Player/BasePart/PVInstance/Players ALL ended
    // up missing their usertype entirely at one point or another — too
    // easy to forget a step that lives in a totally different file from
    // da class itself.
    ClassRegistry::Get().BindAllToLua(*m_lua);
    RegisterPlayerBindings();

    RegisterEngineBindings();
    RegisterHttpBindings();

    // один раз на процесс — реальная изоляция по слоям происходит
    // через LuaSandbox::CreateEnv(layer) на каждый отдельный скрипт
    LuaSandbox::Apply(*m_lua, ScriptLayer::Join);
    KakaScheduler::Get().Init(*m_lua);

    std::cout << "[LuaVM] Initialized\n";
}