/*
    SlateX - 2026
*/
#include "ServerReplicator.hpp"
#include "../Shared/NetworkEvent.hpp"
#include "../Shared/LuaArgSerializer.hpp"
#include "../Shared/Players.hpp"
#include "../../Engine/Scripting/KakaScheduler.hpp"
#include "../../Engine/FemkaDM/DataModel.hpp"
#include "../../Engine/Reflection/Reflection.hpp"
#include <iostream>

ServerReplicator* ServerReplicator::s_active = nullptr;

ServerReplicator::ServerReplicator(std::unique_ptr<ITransport> Transport)
    : ReplicatorBase(std::move(Transport)) {
    m_log = slDBG::NewLog("Logs/ServerReplicator.log", "ServerReplicator");
}

bool ServerReplicator::Start(uint16_t Port) {
    bool Ok = m_transport->StartServer(Port);
    if (Ok) {
        s_active = this;
        if (m_log) m_log->Info("Server started", std::to_string(Port));
        WatchRoot(DataModel::Get());
    }
    return Ok;
}

// --- tree watching ---

void ServerReplicator::WatchRoot(InstanceRef Root) {
    Root->OnChildAdded = [this](InstanceRef Child) {
        WatchAndMaybeBroadcast(Child, /*Broadcast=*/true);
    };

    // whatever's already sitting under root (da default services) — just
    // watch it, dont broadcast, nobody's connected yet at this point
    for (auto& Child : Root->GetChildren())
        WatchAndMaybeBroadcast(Child, /*Broadcast=*/false);
}

void ServerReplicator::WatchAndMaybeBroadcast(InstanceRef Inst, bool Broadcast) {
    // FilterMode::Server means it never leaves the server, da whole subtree
    // included — dont assign a NetId, dont hook anything, dont recurse
    if (Inst->GetFilterMode() == FilterMode::Server) {
        std::cout << "[ServerReplicator] WatchAndMaybeBroadcast: skipping '" << Inst->GetName()
                   << "' (" << Inst->GetClassName() << "), FilterMode::Server\n";
        return;
    }

    bool WasNew = Inst->GetNetId() == 0;
    if (WasNew)
        Inst->SetNetId(AllocateNetId());

    std::cout << "[ServerReplicator] watching '" << Inst->GetName()
               << "' (" << Inst->GetClassName() << "), NetId=" << Inst->GetNetId()
               << (WasNew ? " (new)" : " (already had it)")
               << ", broadcast=" << (Broadcast ? "true" : "false") << "\n";

    Inst->OnChildAdded = [this](InstanceRef Child) {
        WatchAndMaybeBroadcast(Child, /*Broadcast=*/true);
    };
    Inst->OnDestroyed = [this](InstanceRef Removed) {
        BroadcastInstanceRemoved(Removed);
    };
    Inst->OnChanged = [this, Inst](const std::string& PropName) {
        BroadcastInstanceChanged(Inst, PropName);
    };

    if (Broadcast)
        BroadcastInstanceAdded(Inst);

    // recurse into whatever was already parented under Inst before it
    // showed up here — Clone()/manual SetParent chains can hand us a whole
    // pre-built subtree in one AddChild call, gotta catch all of it
    for (auto& Child : Inst->GetChildren())
        WatchAndMaybeBroadcast(Child, Broadcast);
}

// --- snapshot ---

namespace {
    void CollectSnapshot(InstanceRef Node, Serializer& S, uint32_t& Count) {
        for (auto& Child : Node->GetChildren()) {
            if (Child->GetNetId() != 0) {
                auto Parent = Child->GetParent();
                S.WriteUInt32(Child->GetNetId());
                S.WriteUInt32(Parent ? Parent->GetNetId() : 0);
                S.WriteString(Child->GetClassName());
                S.WriteString(Child->GetName());
                Count++;
            }
            CollectSnapshot(Child, S, Count);
        }
    }
}

void ServerReplicator::SendSnapshotTo(ConnId Conn) {
    Serializer Entries;
    uint32_t Count = 0;
    CollectSnapshot(DataModel::Get(), Entries, Count);

    std::cout << "[ServerReplicator] Sending snapshot to " << Conn
               << ", " << Count << " entries\n";

    Serializer S;
    S.WriteUInt32(Count);
    S.WriteRawBuffer(Entries.GetBuffer());
    m_transport->SendTo(Conn, PacketSignal::Snapshot, S);

    Serializer Done;
    m_transport->SendTo(Conn, PacketSignal::SnapshotFinished, Done);
}

void ServerReplicator::BroadcastInstanceAdded(InstanceRef Inst) {
    auto Parent = Inst->GetParent();
    Serializer S;
    S.WriteUInt32(Inst->GetNetId());
    S.WriteUInt32(Parent ? Parent->GetNetId() : 0);
    S.WriteString(Inst->GetClassName());
    S.WriteString(Inst->GetName());
    m_transport->SendToAll(PacketSignal::InstanceAdded, S);
}

void ServerReplicator::BroadcastInstanceRemoved(InstanceRef Inst) {
    if (Inst->GetNetId() == 0) return; // never replicated, nothing to tell anyone

    Serializer S;
    S.WriteUInt32(Inst->GetNetId());
    m_transport->SendToAll(PacketSignal::InstanceRemoved, S);
}

void ServerReplicator::BroadcastInstanceChanged(InstanceRef Inst, const std::string& PropName) {
    if (Inst->GetNetId() == 0) return; // not even replicated, nothing to tell anyone

    auto& Lua = KakaScheduler::Get().GetLua();
    sol::object Value;

    // Name aint a Reflection property — its built straight into Instance's
    // own sol2 usertype, gotta grab it directly instead of walking da chain
    if (PropName == "Name") {
        Value = sol::make_object(Lua, Inst->GetName());
    } else {
        std::string ClassName = Inst->GetClassName();
        const PropertyDescriptor* Prop = nullptr;
        while (!ClassName.empty()) {
            const ClassDescriptor* Desc = ClassRegistry::Get().Find(ClassName);
            if (!Desc) break;
            Prop = Desc->FindProp(PropName);
            if (Prop) break;
            ClassName = Desc->baseClassName;
        }
        // not a registered property at all, or marked non-replicated on
        // purpose (replicated=false) — nothing to broadcast
        if (!Prop || !Prop->replicated || !Prop->getLua) return;
        Value = Prop->getLua(Inst.get(), Lua.lua_state());
    }

    Serializer S;
    S.WriteUInt32(Inst->GetNetId());
    S.WriteString(PropName);
    LuaArgSerializer::SerializeArg(S, Value);
    m_transport->SendToAll(PacketSignal::InstanceChanged, S);
}

void ServerReplicator::OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) {
    switch (Signal) {
    case PacketSignal::Ping: {
        // da polite response
        Serializer S;
        m_transport->SendTo(From, PacketSignal::Pong, S);
        break;
    }
    case PacketSignal::RemoteEvent: {
        uint32_t netId = D.ReadUInt32();
        auto args = LuaArgSerializer::DeserializeArgs(D, KakaScheduler::Get().GetLua());
        NetworkEvent::DispatchFire(netId, From, args, /*IsServerSide=*/true);
        break;
    }
    case PacketSignal::RemoteFunction: {
        uint8_t subtype = D.ReadByte();
        uint32_t netId  = D.ReadUInt32();
        uint64_t reqId  = D.ReadUInt64();
        auto args = LuaArgSerializer::DeserializeArgs(D, KakaScheduler::Get().GetLua());
        if (subtype == 0)
            NetworkEvent::DispatchInvokeRequest(netId, From, reqId, args, /*IsServerSide=*/true);
        else
            NetworkEvent::DispatchInvokeResponse(reqId, args);
        break;
    }
    default:
        if (m_log)
            m_log->Warn("Unknown packet", std::to_string(static_cast<int>(Signal)));
        break;
    }
}

void ServerReplicator::OnClientConnected(ConnId Conn) {
    std::cout << "[ServerReplicator] Client connected: " << Conn << "\n";
    if (m_log) m_log->Info("Client connected", std::to_string(Conn));

    // da welcome message
    Serializer S;
    S.WriteString("hello from server");
    m_transport->SendTo(Conn, PacketSignal::Welcome, S);

    // create da Player for dis connection — naming is just a placeholder
    // ("PlayerNNN") since dere's no real auth handshake feeding a username
    // into da join flow yet, dat's a separate piece of work
    auto PlayersService = std::dynamic_pointer_cast<Players>(
        DataModel::Get()->FindFirstChild("Players"));

    if (PlayersService) {
        auto NewPlayer = std::make_shared<Player>();
        NewPlayer->SetConnId(static_cast<uint64_t>(Conn));
        NewPlayer->SetName("Player" + std::to_string(Conn));
        NewPlayer->SetParent(PlayersService);
        // ^ AddChild fires here -> Players's OnChildAdded (hooked back in
        // WatchRoot, Players is one of da default services) ->
        // WatchAndMaybeBroadcast assigns a NetId and broadcasts
        // InstanceAdded to EVERY connected client, dis brand new one
        // included. da snapshot below also lists dis same Player — see
        // ApplyInstanceAdded's "already known" guard on da client side for
        // why dat double-delivery is harmless, not a duplicate.

        PlayersService->PlayerAdded.Fire({
            ClassRegistry::Get().Push(NewPlayer, KakaScheduler::Get().GetLua().lua_state())
        });

        Serializer LP;
        LP.WriteUInt32(NewPlayer->GetNetId());
        m_transport->SendTo(Conn, PacketSignal::LocalPlayer, LP);
    } else {
        std::cerr << "[ServerReplicator] game.Players isnt a real Players object, skipping Player creation\n";
    }

    SendSnapshotTo(Conn);
}

void ServerReplicator::OnClientDisconnected(ConnId Conn, const std::string& Reason) {
    std::cout << "[ServerReplicator] Client disconnected: " << Reason << "\n";
    if (m_log) m_log->Info("Client disconnected", Reason);

    auto DisconnectedPlayer = Player::FindByConnId(static_cast<uint64_t>(Conn));
    if (DisconnectedPlayer) {
        auto PlayersService = std::dynamic_pointer_cast<Players>(
            DataModel::Get()->FindFirstChild("Players"));
        if (PlayersService) {
            PlayersService->PlayerRemoving.Fire({
                ClassRegistry::Get().Push(DisconnectedPlayer, KakaScheduler::Get().GetLua().lua_state())
            });
        }
        // Destroy() fires OnDestroyed -> BroadcastInstanceRemoved, already
        // wired generically back in WatchAndMaybeBroadcast
        DisconnectedPlayer->Destroy();
    }
}