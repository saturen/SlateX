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

private:
    std::unordered_map<std::string, ClassDescriptor> m_classes;
    std::unordered_map<std::string, Factory>         m_factories;
};

// ---------------------------------------------------------
// Форвард-декларация — реальные специализации лежат в PropTraits.hpp.
// Без неё компилятор не знает, что PropTraits — это шаблон, и
// PropTraits<T>::kType ниже не парсится как template-id вообще
// (классическая ловушка двухфазного поиска имён в шаблонах).
// ---------------------------------------------------------
template<typename T>
struct PropTraits;

// ---------------------------------------------------------
// Типобезопасный helper — генерит lua get/set + serialize по T.
// Требует специализацию PropTraits<T> (см. PropTraits.hpp):
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
// Макросы регистрации — тонкая обёртка, основная работа в MakeProperty<>
// ---------------------------------------------------------
#define SLATE_CLASS_BEGIN(ClassType, BaseName)                  \
    static bool __slate_register_##ClassType = [] {             \
        ClassDescriptor __desc;                                 \
        __desc.className     = #ClassType;                      \
        __desc.baseClassName = BaseName;

#define SLATE_PROP(ClassType, Name, Getter, Setter)              \
        __desc.members.push_back(                                \
            MakeProperty<ClassType>(Name, &ClassType::Getter, &ClassType::Setter));

#define SLATE_PROP_NOREP(ClassType, Name, Getter, Setter)         \
        __desc.members.push_back(                                 \
            MakeProperty<ClassType>(Name, &ClassType::Getter, &ClassType::Setter, false));

#define SLATE_CLASS_END(ClassType)                                \
        ClassRegistry::Get().Register(std::move(__desc));         \
        ClassRegistry::Get().RegisterFactory(#ClassType,          \
            [] { return std::make_shared<ClassType>(); });        \
        return true;                                               \
    }();