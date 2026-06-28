/*
    SlateX - 2026
*/
#pragma once
#include "../../Engine/FemkaDM/Instance.hpp"
#include "../Transport/ITransport.hpp"
#include "Signal.hpp"
#include <cstdint>
#include <vector>

// NetType / NetProto — see the original spec:
//   .Type = Enum.NetType.Function/Remote/Bindable
//   .Protocol = Enum.NetProto.UDP/TCP
enum class NetType : uint8_t {
    Bindable = 0, // local only, never touches the network
    Remote   = 1, // fire-and-forget, client<->server
    Function = 2, // request/response with a return value, client<->server
};

enum class NetProto : uint8_t {
    UDP = 0, // unreliable — fine for high-frequency stuff like positions
    TCP = 1, // reliable — for anything that must arrive (purchases, damage)
};

// NetworkEvent — the Lua-facing FireServer/FireClient/InvokeServer/etc API.
// Lives in Network/ (not Engine/FemkaDM/) because it needs to reach a live
// ServerReplicator/ClientReplicator to actually send anything, and Engine
// can't depend on Network (see Runtime/Engine.hpp).
//
// OnServer/server-side InvokeServer handlers get a real Player as their
// first arg now (see PushPlayerArg in NetworkEvent.cpp) — resolved through
// Player::FindByConnId, nil if somehow nobody's hooked up yet.
class NetworkEvent : public Instance {
public:
    NetworkEvent();

    bool IsA(const std::string& ClassName) const override;

    NetType  GetType() const { return m_type; }
    void     SetType(NetType v) { m_type = v; }
    NetProto GetProtocol() const { return m_protocol; }
    void     SetProtocol(NetProto v) { m_protocol = v; }

    // --- signals (see Signal.hpp for SetConnection/DoOnce/Wait/Fire) ---
    Signal Fired;    // Bindable
    Signal OnServer; // Remote/Function — server-side handlers
    Signal OnClient; // Remote/Function — client-side handlers

    // --- behavior, bound into the NetworkEvent Lua usertype ---
    void FireBindable(sol::variadic_args Args);
    void FireServer(sol::variadic_args Args);                    // client -> server
    void FireClient(uint64_t TargetConn, sol::variadic_args Args); // server -> one client
    void FireAllClients(sol::variadic_args Args);                 // server -> everyone

    // both of these are bound via sol::yielding — see LuaVM registration
    sol::object InvokeServer(sol::variadic_args Args);
    sol::object InvokeClient(uint64_t TargetConn, sol::variadic_args Args);

    // --- called by ServerReplicator/ClientReplicator on incoming packets ---
    static void DispatchFire(uint32_t NetId, ConnId From, std::vector<sol::object> Args, bool IsServerSide);
    static void DispatchInvokeRequest(uint32_t NetId, ConnId From, uint64_t RequestId,
                                       std::vector<sol::object> Args, bool IsServerSide);
    static void DispatchInvokeResponse(uint64_t RequestId, std::vector<sol::object> Args);

private:
    void SendInvokeResponse(ConnId Target, uint64_t RequestId,
                            std::vector<sol::object> Args, bool IsServerSide);

    NetType  m_type     = NetType::Remote;
    NetProto m_protocol = NetProto::TCP;
};