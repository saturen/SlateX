/*
    SlateX - 2026
*/
#include "Reflection.hpp"
#include <iostream>

// these used to be static (internal-linkage) functions hidden inside
// Runtime/LuaVM.cpp — moved here, given real linkage, because da
// SLATE_CLASS_END macro (expanded in OTHER .cpp files, all over Engine/
// and Network/) needs to reattach dem on every class's usertype. sol2
// doesnt inherit meta_functions through sol::base_classes, every single
// derived class needs da SAME __index/__newindex reattached by hand —
// see da BindLua comment in Reflection.hpp for da whole story.

sol::object PushInstance(const InstanceRef& Ref, lua_State* L) {
    return ClassRegistry::Get().Push(Ref, L);
}

sol::object InstanceIndex(Instance& self, const std::string& key, sol::this_state s) {
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
    if (child) return PushInstance(child, s.lua_state());

    return sol::lua_nil;
}

void InstanceNewIndex(Instance& self, const std::string& key, sol::object value) {
    std::string className = self.GetClassName();
    while (!className.empty()) {
        const ClassDescriptor* desc = ClassRegistry::Get().Find(className);
        if (!desc) break;
        const PropertyDescriptor* prop = desc->FindProp(key);
        if (prop && prop->setLua) {
            if (self.IsPropertyLocked(key)) {
                std::cerr << "[Reflection] Property '" << key << "' is locked on "
                          << self.GetClassName() << "\n";
                return;
            }
            prop->setLua(&self, value);
            return;
        }
        className = desc->baseClassName;
    }
    std::cerr << "[Reflection] Unknown property '" << key
              << "' on " << self.GetClassName() << "\n";
}