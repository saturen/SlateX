/*
    SlateX - 2026
*/
#include "Players.hpp"
#include "../../Engine/Reflection/Reflection.hpp"

void Players::RegisterClass() {
    ClassDescriptor Desc;
    Desc.className     = "Players";
    Desc.baseClassName = "Instance";
    ClassRegistry::Get().Register(std::move(Desc));
    ClassRegistry::Get().RegisterFactory("Players",
        [] { return std::make_shared<Players>(); });

    // without dis, Instance.new("Players") (or anything pushing a Players
    // InstanceRef into Lua) shows up typed as plain Instance —
    // PlayerAdded/PlayerRemoving/LocalPlayer invisible, see da PushFn
    // comment in Reflection.hpp
    ClassRegistry::Get().RegisterPush("Players",
        [](InstanceRef Ref, sol::state_view L) -> sol::object {
            return sol::make_object(L,
                std::static_pointer_cast<Players>(Ref));
        });

    // Players has native sol2 members of its own (Signal references,
    // GetPlayers method) — cant use da macro's auto-generated BindLua
    // (dat one's only for plain Reflection-property classes), gotta write
    // it by hand, same as NetworkEvent does in its own .cpp.
    //
    // THIS USERTYPE NEVER EXISTED until now, by da way — Players was
    // riding on sol2's empty auto-fallback registration same as Player
    // was before its fix, meaning game.Players.PlayerAdded was ALSO
    // silently broken dis whole time, nobody happened to test it yet.
    ClassRegistry::Get().RegisterBindLua("Players",
        [](sol::state& Lua) {
            Lua.new_usertype<Players>("Players",
                sol::no_constructor,
                sol::base_classes, sol::bases<Instance>(),

                // sol2 doesnt inherit meta_functions through base_classes —
                // same reattachment every single derived usertype needs
                sol::meta_function::index,     InstanceIndex,
                sol::meta_function::new_index, InstanceNewIndex,

                "PlayerAdded",   sol::property([](Players& self) -> Signal& { return self.PlayerAdded; }),
                "PlayerRemoving", sol::property([](Players& self) -> Signal& { return self.PlayerRemoving; }),

                "LocalPlayer", sol::property(
                    [](Players& self, sol::this_state s) -> sol::object {
                        return PushInstance(self.GetLocalPlayer(), s.lua_state());
                    },
                    [](Players& self, InstanceRef P) { self.SetLocalPlayer(P); }
                ),

                "GetPlayers", [](Players& self, sol::this_state s) {
                    std::vector<sol::object> Out;
                    for (auto& P : self.GetPlayers())
                        Out.push_back(PushInstance(P, s.lua_state()));
                    return Out;
                }
            );
        });
}