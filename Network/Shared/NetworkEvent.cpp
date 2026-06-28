/*
    SlateX - 2026
*/
#include "NetworkEvent.hpp"
#include "LuaArgSerializer.hpp"
#include "../../Engine/Scripting/KakaScheduler.hpp"
#include "../../Engine/Reflection/Reflection.hpp"
#include "../../Engine/FemkaDM/Player.hpp"
#include "../Replicator/ServerReplicator.hpp"
#include "../Replicator/ClientReplicator.hpp"
#include "../Transport/Serializer.hpp"
#include <iostream>

namespace {
    // no Reflection/PropertyDescriptor entries for this class on purpose —
    // Type/Protocol/Fire*/Invoke* all live on NetworkEvent's own sol
    // usertype, registered right below via BindLua — this just makes
    // Instance.new("NetworkEvent") and ClassRegistry::Find/Create aware
    // that the class exists at all.
    bool RegisterNetworkEventFactory = [] {
        ClassDescriptor Desc;
        Desc.className     = "NetworkEvent";
        Desc.baseClassName = "Instance";
        ClassRegistry::Get().Register(std::move(Desc));
        ClassRegistry::Get().RegisterFactory("NetworkEvent",
            [] { return std::make_shared<NetworkEvent>(); });

        // see da PushFn comment in Reflection.hpp — without dis,
        // Instance.new("NetworkEvent") hands da object to Lua typed as
        // plain Instance, and every native sol2 member NetworkEvent has
        // (Type/Fired/OnServer/Fire*/Invoke*) becomes invisible (sol2
        // picks da metatable by static C++ type, not by RTTI)
        ClassRegistry::Get().RegisterPush("NetworkEvent",
            [](InstanceRef ref, sol::state_view L) -> sol::object {
                return sol::make_object(L,
                    std::static_pointer_cast<NetworkEvent>(ref));
            });

        // NetworkEvent has native sol2 members of its own (Type/Protocol/
        // Signal refs/Fire*/Invoke*) — cant use da macro's auto-generated
        // BindLua (dat one's only for plain Reflection-property classes
        // like Player/BasePart), gotta write it by hand. Used to live in
        // Runtime/LuaVM.cpp as RegisterNetworkEventBindings — moved here
        // so it lives next to da class it actually binds, instead of in a
        // separate file someone has to remember to also edit.
        ClassRegistry::Get().RegisterBindLua("NetworkEvent",
            [](sol::state& Lua) {
                Lua.new_usertype<NetworkEvent>("NetworkEvent",
                    sol::no_constructor,
                    sol::base_classes, sol::bases<Instance>(),

                    // sol2 does NOT inherit meta_functions through
                    // sol::base_classes — gotta reattach dese on every
                    // single derived usertype, no exceptions
                    sol::meta_function::index,     InstanceIndex,
                    sol::meta_function::new_index, InstanceNewIndex,

                    "Type", sol::property(
                        [](NetworkEvent& self) { return static_cast<int>(self.GetType()); },
                        [](NetworkEvent& self, int v) { self.SetType(static_cast<NetType>(v)); }
                    ),
                    "Protocol", sol::property(
                        [](NetworkEvent& self) { return static_cast<int>(self.GetProtocol()); },
                        [](NetworkEvent& self, int v) { self.SetProtocol(static_cast<NetProto>(v)); }
                    ),

                    "Fired",    sol::property([](NetworkEvent& self) -> Signal& { return self.Fired; }),
                    "OnServer", sol::property([](NetworkEvent& self) -> Signal& { return self.OnServer; }),
                    "OnClient", sol::property([](NetworkEvent& self) -> Signal& { return self.OnClient; }),

                    "Fire",           &NetworkEvent::FireBindable,
                    "FireServer",      &NetworkEvent::FireServer,
                    "FireClient",      &NetworkEvent::FireClient,
                    "FireAllClients",  &NetworkEvent::FireAllClients,

                    // both suspend da calling task until da matching
                    // response/timeout arrives — see
                    // KakaScheduler::SuspendCurrentTask/FulfillRequest
                    "InvokeServer", sol::yielding(&NetworkEvent::InvokeServer),
                    "InvokeClient", sol::yielding(&NetworkEvent::InvokeClient)
                );

                // Enum.NetType.Bindable/Remote/Function, Enum.NetProto.UDP/
                // TCP — plain integer constants matching da C++ enum values
                sol::table enumTable = Lua["Enum"].get_or_create<sol::table>();

                sol::table netTypeTbl  = Lua.create_table();
                netTypeTbl["Bindable"]  = static_cast<int>(NetType::Bindable);
                netTypeTbl["Remote"]    = static_cast<int>(NetType::Remote);
                netTypeTbl["Function"]  = static_cast<int>(NetType::Function);
                enumTable["NetType"]    = netTypeTbl;

                sol::table netProtoTbl = Lua.create_table();
                netProtoTbl["UDP"]      = static_cast<int>(NetProto::UDP);
                netProtoTbl["TCP"]      = static_cast<int>(NetProto::TCP);
                enumTable["NetProto"]   = netProtoTbl;

                Lua["Enum"] = enumTable;
            });

        return true;
    }();

    // Player arg pushed as da first param to OnServer handlers (FindByConnId
    // returns nullptr right at da edge of connect/disconnect — push nil
    // rather than secretly falling back to a raw number, scripts shouldnt
    // have to guard against two different types for "who fired dis")
    sol::object PushPlayerArg(ConnId From, sol::state& Lua) {
        auto Player_ = Player::FindByConnId(static_cast<uint64_t>(From));
        if (!Player_) return sol::make_object(Lua, sol::lua_nil);
        return ClassRegistry::Get().Push(Player_, Lua.lua_state());
    }
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
    client->GetTransport()->Send(PacketSignal::RemoteEvent, S, GetProtocol() == NetProto::TCP);
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
    server->GetTransport()->SendTo(static_cast<ConnId>(TargetConn), PacketSignal::RemoteEvent, S, GetProtocol() == NetProto::TCP);
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
    server->GetTransport()->SendToAll(PacketSignal::RemoteEvent, S, GetProtocol() == NetProto::TCP);
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
        fullArgs.push_back(PushPlayerArg(From, KakaScheduler::Get().GetLua()));
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
        fullArgs.push_back(PushPlayerArg(From, KakaScheduler::Get().GetLua()));
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