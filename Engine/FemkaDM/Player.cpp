/*
    SlateX - 2026
*/
#include "Player.hpp"
#include "../Reflection/Reflection.hpp"

// file-static, same deal as Instance's own NetId registry — keeps it out
// of every .o that includes Player.hpp
static std::unordered_map<uint64_t, std::weak_ptr<Player>> g_connRegistry;

void Player::SetConnId(uint64_t Id) {
    if (m_connId != 0) {
        auto It = g_connRegistry.find(m_connId);
        if (It != g_connRegistry.end()) g_connRegistry.erase(It);
    }

    m_connId = Id;

    if (Id != 0)
        g_connRegistry[Id] = std::static_pointer_cast<Player>(shared_from_this());
}

std::shared_ptr<Player> Player::FindByConnId(uint64_t Id) {
    auto It = g_connRegistry.find(Id);
    if (It == g_connRegistry.end()) return nullptr;
    return It->second.lock();
}

// no SLATE_PROP lines — Player has no extra value-properties of its own
// right now (m_connId is server-internal only, never Lua-facing). Da
// usertype generated here gets a plain to_string bolted on top
// afterward, from LuaVM.cpp, since da macro doesnt have a slot for "one
// extra meta_function" — see RegisterPlayerExtras in LuaVM.cpp.
SLATE_CLASS_BEGIN(Player, Instance)
SLATE_CLASS_END(Player, Instance)