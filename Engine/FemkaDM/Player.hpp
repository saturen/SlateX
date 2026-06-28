/*
    SlateX - 2026
*/
#pragma once
#include "Instance.hpp"
#include <unordered_map>

// Player — one connected client. Server creates one of these per connection
// (see ServerReplicator::OnClientConnected) and parents it under
// game.Players; replicates like any other Instance from there.
//
// m_connId is da only server-only-meaningful bit here — it's how
// NetworkEvent::DispatchFire/DispatchInvokeRequest figure out WHICH Player
// fired something (used to be a raw ConnId number passed straight to Lua,
// see da old comment in NetworkEvent.cpp — this finally closes dat gap).
//
// Plain uint64_t instead of Network's ConnId typedef on purpose — Engine
// aint allowed to depend on Network (same reason Engine.cpp lives in
// Runtime, see Runtime/Engine.hpp), they're da same underlying type anyway.
class Player : public Instance {
public:
    Player() : Instance("Player") {}

    bool IsA(const std::string& ClassName) const override {
        if (ClassName == "Player") return true;
        return Instance::IsA(ClassName);
    }

    uint64_t GetConnId() const { return m_connId; }
    void     SetConnId(uint64_t Id);

    // server-side only — finds da Player for a given connection, or
    // nullptr if nobody's hooked one up yet (or it already disconnected)
    static std::shared_ptr<Player> FindByConnId(uint64_t Id);

private:
    uint64_t m_connId = 0;
};