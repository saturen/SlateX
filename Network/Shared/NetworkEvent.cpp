/*
    SlateX - 2026
*/
#include "NetworkEvent.hpp"
#include "LuaArgSerializer.hpp"
#include "../../Engine/Scripting/KakaScheduler.hpp"
#include "../../Engine/Reflection/Reflection.hpp"
#include "../Replicator/ServerReplicator.hpp"
#include "../Replicator/ClientReplicator.hpp"
#include "../Transport/Serializer.hpp"
#include <iostream>

namespace {
    // no Reflection/PropertyDescriptor entries for this class on purpose —
    // Type/Protocol/Fire*/Invoke* all live on NetworkEvent's own sol
    // usertype (see LuaVM registration), this just makes Instance.new("NetworkEvent")
    // and ClassRegistry::Find/Create aware that the class exists at all.
    bool RegisterNetworkEventFactory = [] {
        ClassDescriptor Desc;
        Desc.className     = "NetworkEvent";
        Desc.baseClassName = "Instance";
        ClassRegistry::Get().Register(std::move(Desc));
        ClassRegistry::Get().RegisterFactory("NetworkEvent",
            [] { return std::make_shared<NetworkEvent>(); });
        return true;
    }();
}

NetworkEvent::NetworkEvent() : Instance("NetworkEvent") {}

bool NetworkEvent::IsA(const std::string& ClassName) const {
    if (ClassName == "NetworkEvent") return true;
    return Instance::IsA(ClassName);
}

// --- local-only ---

void NetworkEvent::FireBindable(sol::variadic_args Args) {
    std::vector<sol::object> argList(Args.begin(), Args.end());
    Fired.Fire(argList);
}

// --- fire-and-forget, over the network ---

void NetworkEvent::FireServer(sol::variadic_args Args) {
    auto* client = ClientReplicator::GetActive();
    if (!client) {
        std::cerr << "[NetworkEvent] FireServer: no active client connection\n";
        return;
    }
    std::vector<sol::object> argList(Args.begin(), Args.end());
    Serializer S;
    S.WriteUInt32(GetNetId());
    LuaArgSerializer::SerializeArgs(S, argList);
    client->GetTransport()->Send(PacketSignal::RemoteEvent, S);
}

void NetworkEvent::FireClient(uint64_t TargetConn, sol::variadic_args Args) {
    auto* server = ServerReplicator::GetActive();
    if (!server) {
        std::cerr << "[NetworkEvent] FireClient: no active server\n";
        return;
    }
    std::vector<sol::object> argList(Args.begin(), Args.end());
    Serializer S;
    S.WriteUInt32(GetNetId());
    LuaArgSerializer::SerializeArgs(S, argList);
    server->GetTransport()->SendTo(static_cast<ConnId>(TargetConn), PacketSignal::RemoteEvent, S);
}

void NetworkEvent::FireAllClients(sol::variadic_args Args) {
    auto* server = ServerReplicator::GetActive();
    if (!server) {
        std::cerr << "[NetworkEvent] FireAllClients: no active server\n";
        return;
    }
    std::vector<sol::object> argList(Args.begin(), Args.end());
    Serializer S;
    S.WriteUInt32(GetNetId());
    LuaArgSerializer::SerializeArgs(S, argList);
    server->GetTransport()->SendToAll(PacketSignal::RemoteEvent, S);
}

// --- request/response, over the network ---

sol::object NetworkEvent::InvokeServer(sol::variadic_args Args) {
    auto& Lua = KakaScheduler::Get().GetLua();

    auto* client = ClientReplicator::GetActive();
    if (!client) {
        std::cerr << "[NetworkEvent] InvokeServer: no active client connection\n";
        return sol::make_object(Lua, sol::lua_nil);
    }

    uint64_t reqId = KakaScheduler::Get().SuspendCurrentTask();

    std::vector<sol::object> argList(Args.begin(), Args.end());
    Serializer S;
    S.WriteByte(0); // subtype: Request
    S.WriteUInt32(GetNetId());
    S.WriteUInt64(reqId);
    LuaArgSerializer::SerializeArgs(S, argList);
    client->GetTransport()->Send(PacketSignal::RemoteFunction, S);

    // unused — the real return value is delivered later, via FulfillRequest
    // resuming this same call with the actual response args
    return sol::make_object(Lua, sol::lua_nil);
}

sol::object NetworkEvent::InvokeClient(uint64_t TargetConn, sol::variadic_args Args) {
    auto& Lua = KakaScheduler::Get().GetLua();

    auto* server = ServerReplicator::GetActive();
    if (!server) {
        std::cerr << "[NetworkEvent] InvokeClient: no active server\n";
        return sol::make_object(Lua, sol::lua_nil);
    }

    uint64_t reqId = KakaScheduler::Get().SuspendCurrentTask();

    std::vector<sol::object> argList(Args.begin(), Args.end());
    Serializer S;
    S.WriteByte(0);
    S.WriteUInt32(GetNetId());
    S.WriteUInt64(reqId);
    LuaArgSerializer::SerializeArgs(S, argList);
    server->GetTransport()->SendTo(static_cast<ConnId>(TargetConn), PacketSignal::RemoteFunction, S);

    return sol::make_object(Lua, sol::lua_nil);
}

// --- incoming packet dispatch (called from ServerReplicator/ClientReplicator) ---

void NetworkEvent::DispatchFire(uint32_t NetId, ConnId From, std::vector<sol::object> Args, bool IsServerSide) {
    auto inst = Instance::FindByNetId(NetId);
    auto ev = std::dynamic_pointer_cast<NetworkEvent>(inst);
    if (!ev) {
        std::cerr << "[NetworkEvent] Fire for unknown NetId " << NetId << "\n";
        return;
    }

    std::vector<sol::object> fullArgs;
    if (IsServerSide) {
        // NOTE: no real Player system yet — raw ConnId stands in for "player"
        // until one exists. Scripts get a number here, not an Instance.
        fullArgs.push_back(sol::make_object(KakaScheduler::Get().GetLua(), static_cast<double>(From)));
    }
    for (auto& a : Args) fullArgs.push_back(a);

    Signal& target = IsServerSide ? ev->OnServer : ev->OnClient;
    target.Fire(fullArgs);
}

void NetworkEvent::DispatchInvokeRequest(uint32_t NetId, ConnId From, uint64_t RequestId,
                                          std::vector<sol::object> Args, bool IsServerSide) {
    auto inst = Instance::FindByNetId(NetId);
    auto ev = std::dynamic_pointer_cast<NetworkEvent>(inst);
    if (!ev) {
        std::cerr << "[NetworkEvent] Invoke request for unknown NetId " << NetId << "\n";
        return;
    }

    Signal& target = IsServerSide ? ev->OnServer : ev->OnClient;
    sol::function handler = target.GetFirstHandler();
    if (!handler.valid()) {
        std::cerr << "[NetworkEvent] Invoke request with no handler set — no response will be sent\n";
        return;
    }

    std::vector<sol::object> fullArgs;
    if (IsServerSide)
        fullArgs.push_back(sol::make_object(KakaScheduler::Get().GetLua(), static_cast<double>(From)));
    for (auto& a : Args) fullArgs.push_back(a);

    // ev (shared_ptr) is captured by value so the NetworkEvent stays alive
    // for as long as this invoke is pending, even if it Destroy()s elsewhere
    KakaScheduler::Get().SpawnWithCallback(handler, fullArgs,
        [ev, From, RequestId, IsServerSide](std::vector<sol::object> result) {
            ev->SendInvokeResponse(From, RequestId, result, IsServerSide);
        },
        "InvokeHandler", ScriptLayer::Game
    );
}

void NetworkEvent::DispatchInvokeResponse(uint64_t RequestId, std::vector<sol::object> Args) {
    bool ok = KakaScheduler::Get().FulfillRequest(RequestId, Args);
    if (!ok)
        std::cerr << "[NetworkEvent] Invoke response for unknown/expired request #" << RequestId << "\n";
}

void NetworkEvent::SendInvokeResponse(ConnId Target, uint64_t RequestId,
                                       std::vector<sol::object> Args, bool IsServerSide) {
    Serializer S;
    S.WriteByte(1); // subtype: Response
    S.WriteUInt32(GetNetId());
    S.WriteUInt64(RequestId);
    LuaArgSerializer::SerializeArgs(S, Args);

    if (IsServerSide) {
        auto* server = ServerReplicator::GetActive();
        if (server) server->GetTransport()->SendTo(Target, PacketSignal::RemoteFunction, S);
    } else {
        auto* client = ClientReplicator::GetActive();
        if (client) client->GetTransport()->Send(PacketSignal::RemoteFunction, S);
    }
}