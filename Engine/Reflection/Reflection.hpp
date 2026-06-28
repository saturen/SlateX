/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <sol/sol.hpp>
#include "../FemkaDM/Instance.hpp"

// Reflection — система описания свойств классов.
// Макросы как в gen2 (PropertyDescriptor), но генерация
// serialize/deserialize/getLua/setLua через шаблон по типу,
// а не вручную лямбдами в каждом блоке.

enum class PropType : uint32_t {
    Bool, Int, Float, Double, String, Vector3, CFrame, Color3, InstanceRef_
};

struct PropertyDescriptor {
    std::string name;
    PropType    type;
    bool        replicated = true;

    std::function<sol::object(Instance*, lua_State*)>  getLua;
    std::function<void(Instance*, sol::object)>        setLua;
    std::function<std::string(Instance*)>              serialize;
    std::function<void(Instance*, const std::string&)> deserialize;
};

struct ClassDescriptor {
    std::string                     className;
    std::string                     baseClassName;
    std::vector<PropertyDescriptor> members;

    const PropertyDescriptor* FindProp(const std::string& Name) const {
        for (auto& p : members)
            if (p.name == Name) return &p;
        return nullptr;
    }
};

// __index/__newindex shared by EVERY Instance subclass — reflection
// properties first (CFrame/Size/Anchored etc), then children by name
// (FindFirstChild). Defined in Reflection.cpp (real linkage, not static)
// because SLATE_CLASS_END expands in dozens of different .cpp files and
// every single one needs to reattach da SAME pair — sol2 doesnt inherit
// meta_functions through sol::base_classes, ever, no exceptions.
sol::object InstanceIndex(Instance& self, const std::string& key, sol::this_state s);
void        InstanceNewIndex(Instance& self, const std::string& key, sol::object value);

// every InstanceRef crossing into Lua MUST go through dis, not a raw
// sol2 return-by-InstanceRef — see ClassRegistry::Push's comment below
sol::object PushInstance(const InstanceRef& Ref, lua_State* L);

class ClassRegistry {
public:
    static ClassRegistry& Get() { static ClassRegistry r; return r; }

    void Register(ClassDescriptor desc) {
        m_classes[desc.className] = std::move(desc);
    }
    const ClassDescriptor* Find(const std::string& Name) const {
        auto it = m_classes.find(Name);
        return it != m_classes.end() ? &it->second : nullptr;
    }

    using Factory = std::function<InstanceRef()>;
    void RegisterFactory(const std::string& Name, Factory f) { m_factories[Name] = std::move(f); }
    InstanceRef Create(const std::string& Name) const {
        auto it = m_factories.find(Name);
        return it != m_factories.end() ? it->second() : nullptr;
    }

    // PushToLua — конвертирует InstanceRef (shared_ptr<Instance>) в sol::object
    // с настоящим динамическим типом (NetworkEvent, BasePart, ...), а не
    // статическим Instance.
    //
    // Зачем это нужно: sol2 привязывает Lua-объект к СТАТИЧЕСКОМУ типу
    // переданного shared_ptr, а не к динамическому через RTTI — даже если
    // и Instance, и NetworkEvent зарегистрированы как usertype с
    // sol::base_classes/sol::bases<>. Любой класс, у которого есть свои
    // НАТИВНЫЕ sol2-члены (методы/свойства, заданные прямо в new_usertype<T>,
    // а не через Reflection::PropertyDescriptor), окажется невидим из Lua,
    // если объект пришёл как generic InstanceRef — ровно это случилось с
    // NetworkEvent.Type/.Fired/:Fire() (см. SLATE_CLASS_END и
    // ClassRegistry::Create — раньше Instance.new() возвращал InstanceRef
    // напрямую, sol2 конвертировал его как Instance).
    //
    // Свойства, идущие через Reflection (CFrame/Size/Anchored у BasePart),
    // эту проблему не имеют — InstanceIndex/InstanceNewIndex резолвят их
    // вручную по GetClassName() (обычный virtual-вызов C++), полностью
    // минуя систему типов sol2.
    using PushFn = std::function<sol::object(InstanceRef, sol::state_view)>;
    void RegisterPush(const std::string& Name, PushFn f) { m_pushFns[Name] = std::move(f); }

    // Возвращает Ref как sol::object правильного динамического типа, если для
    // его класса зарегистрирован PushFn (через SLATE_CLASS_END или вручную,
    // см. NetworkEvent.cpp). Если нет — fallback на обычный push как Instance
    // (безопасно для классов без своих native sol2-членов).
    sol::object Push(const InstanceRef& Ref, sol::state_view L) const {
        if (!Ref) return sol::lua_nil;
        auto it = m_pushFns.find(Ref->GetClassName());
        if (it != m_pushFns.end()) return it->second(Ref, L);
        return sol::make_object(L, Ref);
    }

    // BindLua — actually calls new_usertype<T>(...) for da class, once a
    // real sol::state exists. Cant happen at static-init time (LuaVM::Init
    // hasnt run yet, there's no sol::state to register into), so it's
    // deferred to here and triggered explicitly by BindAllToLua below.
    //
    // SLATE_CLASS_BEGIN/END auto-generates a DEFAULT one for da common case
    // (no native sol2 members of its own, just Reflection properties) —
    // see da macro below. Classes WITH native members (NetworkEvent,
    // Players — Signal references, Fire/Invoke methods) override dis with
    // their own via RegisterBindLua, called from their own .cpp, same as
    // PushFn already works.
    using BindLuaFn = std::function<void(sol::state&)>;
    void RegisterBindLua(const std::string& Name, BindLuaFn f) { m_bindLuaFns[Name] = std::move(f); }

    // calls every registered class's BindLua exactly once, base classes
    // before derived ones — sol2's bases<>() needs da base usertype to
    // already exist, and dis is da ONLY thing dat enforces dat correctly
    // instead of relying on manually getting da order right in LuaVM.cpp
    // (which is exactly how Player/BasePart/PVInstance/Players ended up
    // silently missing their usertype entirely — too easy to forget).
    //
    // "Instance" itself isnt in here — it's da hardcoded root, bound
    // directly in LuaVM.cpp before dis ever runs, since it has real native
    // methods (FindFirstChild/GetChildren/Destroy/...) dat arent
    // Reflection properties and arent auto-generatable by da macro.
    void BindAllToLua(sol::state& Lua) {
        std::unordered_map<std::string, bool> Bound;
        std::function<void(const std::string&)> BindOne = [&](const std::string& Name) {
            if (Bound[Name]) return;
            auto It = m_bindLuaFns.find(Name);
            if (It == m_bindLuaFns.end()) return; // "Instance" itself, or unregistered base
            auto DescIt = m_classes.find(Name);
            if (DescIt != m_classes.end() && !DescIt->second.baseClassName.empty())
                BindOne(DescIt->second.baseClassName);
            It->second(Lua);
            Bound[Name] = true;
        };
        for (auto& [Name, Fn] : m_bindLuaFns) BindOne(Name);
    }

private:
    std::unordered_map<std::string, ClassDescriptor> m_classes;
    std::unordered_map<std::string, Factory>         m_factories;
    std::unordered_map<std::string, PushFn>          m_pushFns;
    std::unordered_map<std::string, BindLuaFn>       m_bindLuaFns;
};

// ---------------------------------------------------------
// Forward-declaration — real realization in PropTraits.hpp
// Без неё компилятор не знает, что PropTraits — это шаблон, и
// PropTraits<T>::kType ниже не парсится как template-id вообще
// (классическая ловушка двухфазного поиска имён в шаблонах).
// ---------------------------------------------------------
template<typename T>
struct PropTraits;

// ---------------------------------------------------------
// type safe helper — generates lua get/set + serialize by T.
// Requires specialization of PropTraits<T> (ex. PropTraits.hpp):
//   PropTraits<T>::kType, ToLua(T, L), FromLua(sol::object),
//   Serialize(T), Deserialize(string)
// ---------------------------------------------------------
template<typename Class, typename T>
PropertyDescriptor MakeProperty(
    const std::string& name,
    T (Class::*getter)() const,
    void (Class::*setter)(T),
    bool replicated = true)
{
    PropertyDescriptor p;
    p.name       = name;
    p.type       = PropTraits<T>::kType;
    p.replicated = replicated;

    p.getLua = [getter](Instance* i, lua_State* L) -> sol::object {
        return PropTraits<T>::ToLua((static_cast<Class*>(i)->*getter)(), L);
    };
    p.setLua = [setter](Instance* i, sol::object v) {
        if (auto val = PropTraits<T>::FromLua(v))
            (static_cast<Class*>(i)->*setter)(*val);
    };
    if (replicated) {
        p.serialize = [getter](Instance* i) {
            return PropTraits<T>::Serialize((static_cast<Class*>(i)->*getter)());
        };
        p.deserialize = [setter](Instance* i, const std::string& data) {
            (static_cast<Class*>(i)->*setter)(PropTraits<T>::Deserialize(data));
        };
    }
    return p;
}

// ---------------------------------------------------------
// macrosses
//
// BaseType is now a real C++ type token (Instance, PVInstance, ...), not
// a quoted string — needed so da auto-generated BindLua below can write
// sol::bases<BaseType>(). #BaseType still stringifies it for da runtime
// className chain-walk (InstanceIndex/InstanceNewIndex), same as before.
// ---------------------------------------------------------
#define SLATE_CLASS_BEGIN(ClassType, BaseType)                  \
    static bool __slate_register_##ClassType = [] {             \
        ClassDescriptor __desc;                                 \
        __desc.className     = #ClassType;                      \
        __desc.baseClassName = #BaseType;

#define SLATE_PROP(ClassType, Name, Getter, Setter)              \
        __desc.members.push_back(                                \
            MakeProperty<ClassType>(Name, &ClassType::Getter, &ClassType::Setter));

#define SLATE_PROP_NOREP(ClassType, Name, Getter, Setter)         \
        __desc.members.push_back(                                 \
            MakeProperty<ClassType>(Name, &ClassType::Getter, &ClassType::Setter, false));

// common case: no native sol2 members of its own (Player, BasePart, ...) —
// just Reflection properties, gets a fully auto-generated usertype.
// Classes WITH native members (NetworkEvent, Players — Signal refs,
// Fire/Invoke methods) dont use dis at all, dey call RegisterBindLua
// demselves from their own .cpp with a custom one, see NetworkEvent.cpp.
#define SLATE_CLASS_END(ClassType, BaseType)                                \
        ClassRegistry::Get().Register(std::move(__desc));         \
        ClassRegistry::Get().RegisterFactory(#ClassType,          \
            [] { return std::make_shared<ClassType>(); });        \
        ClassRegistry::Get().RegisterPush(#ClassType,              \
            [](InstanceRef ref, sol::state_view L) -> sol::object { \
                return sol::make_object(L,                          \
                    std::static_pointer_cast<ClassType>(ref));      \
            });                                                     \
        ClassRegistry::Get().RegisterBindLua(#ClassType,             \
            [](sol::state& Lua) {                                     \
                Lua.new_usertype<ClassType>(#ClassType,                \
                    sol::no_constructor,                                \
                    sol::base_classes, sol::bases<BaseType>(),           \
                    sol::meta_function::index,     InstanceIndex,         \
                    sol::meta_function::new_index, InstanceNewIndex        \
                );                                                          \
            });                                                              \
        return true;                                               \
    }();

// for abstract classes (PVInstance — pure virtual GetPivot/PivotTo, can
// never be Instance.new()'d) — same deal but no factory, since
// std::make_shared<ClassType>() wouldnt even compile for an abstract type.
// Still needs ClassDescriptor (for da Reflection chain-walk) and BindLua
// (so concrete subclasses can sol::bases<> off it) though.
#define SLATE_ABSTRACT_CLASS_END(ClassType, BaseType)                       \
        ClassRegistry::Get().Register(std::move(__desc));                  \
        ClassRegistry::Get().RegisterBindLua(#ClassType,                    \
            [](sol::state& Lua) {                                            \
                Lua.new_usertype<ClassType>(#ClassType,                       \
                    sol::no_constructor,                                       \
                    sol::base_classes, sol::bases<BaseType>(),                  \
                    sol::meta_function::index,     InstanceIndex,                \
                    sol::meta_function::new_index, InstanceNewIndex               \
                );                                                                 \
            });                                                                     \
        return true;                                                       \
    }();