/*
    SlateX - 2026
*/
#include "ClientReplicator.hpp"
#include "../Shared/NetworkEvent.hpp"
#include "../Shared/LuaArgSerializer.hpp"
#include "../Shared/Players.hpp"
#include "../../Engine/Scripting/KakaScheduler.hpp"
#include "../../Engine/FemkaDM/DataModel.hpp"
#include "../../Engine/Reflection/Reflection.hpp"
#include <iostream>

ClientReplicator* ClientReplicator::s_active = nullptr;

ClientReplicator::ClientReplicator(std::unique_ptr<ITransport> Transport)
    : ReplicatorBase(std::move(Transport)) {
    m_log = slDBG::NewLog("Logs/ClientReplicator.log", "ClientReplicator");
}

bool ClientReplicator::Connect(const std::string& Host, uint16_t Port) {
    if (m_log) m_log->Info("Connecting to", Host + ":" + std::to_string(Port));
    bool Ok = m_transport->Connect(Host, Port);
    if (Ok) s_active = this;
    return Ok;
}

void ClientReplicator::ApplyInstanceAdded(uint32_t NetId, uint32_t ParentNetId,
                                           const std::string& ClassName, const std::string& Name) {
    std::cout << "[ClientReplicator]   entry: NetId=" << NetId
               << " ParentNetId=" << ParentNetId
               << " ClassName=" << ClassName
               << " Name=" << Name << "\n";

    InstanceRef Parent = ParentNetId == 0 ? DataModel::Get() : Instance::FindByNetId(ParentNetId);
    if (!Parent) {
        std::cerr << "[ClientReplicator]   -> DROPPED, unknown parent NetId " << ParentNetId << "\n";
        return;
    }

    // already known on this side — a reparent re-fires InstanceAdded for da
    // same NetId (see WatchAndMaybeBroadcast on da server), just move it,
    // dont spawn a duplicate sitting under da new parent too
    if (auto Existing = Instance::FindByNetId(NetId)) {
        std::cout << "[ClientReplicator]   -> already known, reparented under " << Parent->GetName() << "\n";
        Existing->SetParent(Parent);
        return;
    }

    // top-level under game might already be one of da 6 default services
    // CreateDefaultServices made locally on this side too — match by name,
    // dont spawn a duplicate, just give da existing one da right NetId
    if (ParentNetId == 0) {
        auto Existing = Parent->FindFirstChild(Name);
        if (Existing) {
            std::cout << "[ClientReplicator]   -> matched existing default service '"
                       << Name << "', assigned NetId\n";
            Existing->SetNetId(NetId);
            return;
        }
    }

    auto Inst = ClassRegistry::Get().Create(ClassName);
    if (!Inst) {
        std::cerr << "[ClientReplicator]   -> DROPPED, unknown class '" << ClassName << "'\n";
        return;
    }

    Inst->SetName(Name);
    Inst->SetNetId(NetId);
    Inst->SetParent(Parent);
    std::cout << "[ClientReplicator]   -> created fresh '" << Name
               << "' under " << Parent->GetName() << "\n";
}

void ClientReplicator::OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) {
    switch (Signal) {
    case PacketSignal::Welcome: {
        std::string Msg = D.ReadString();
        std::cout << "[ClientReplicator] Welcome from server: " << Msg << "\n";
        if (m_log) m_log->Info("Welcome", Msg);
        break;
    }
    case PacketSignal::Kick: {
        std::string Reason = D.ReadString();
        std::cout << "[ClientReplicator] Kicked: " << Reason << "\n";
        if (m_log) m_log->Warn("Kicked", Reason);
        if (OnDisconnectedFromServer) OnDisconnectedFromServer(Reason);
        break;
    }
    case PacketSignal::Pong:
        // da server is alive
        break;
    case PacketSignal::Snapshot: {
        uint32_t Count = D.ReadUInt32();
        std::cout << "[ClientReplicator] Snapshot received, " << Count << " entries:\n";
        for (uint32_t i = 0; i < Count; i++) {
            uint32_t NetId       = D.ReadUInt32();
            uint32_t ParentNetId = D.ReadUInt32();
            std::string ClassName = D.ReadString();
            std::string Name      = D.ReadString();
            ApplyInstanceAdded(NetId, ParentNetId, ClassName, Name);
        }
        std::cout << "[ClientReplicator] Snapshot applied, " << Count << " instances\n";
        if (m_log) m_log->Info("Snapshot applied", std::to_string(Count));
        break;
    }
    case PacketSignal::SnapshotFinished:
        std::cout << "[ClientReplicator] Snapshot finished\n";
        if (m_log) m_log->Info("Snapshot finished");
        break;
    case PacketSignal::LocalPlayer: {
        uint32_t NetId = D.ReadUInt32();
        auto LocalPlayerInst = std::dynamic_pointer_cast<Player>(Instance::FindByNetId(NetId));
        auto PlayersService  = std::dynamic_pointer_cast<Players>(
            DataModel::Get()->FindFirstChild("Players"));

        if (!LocalPlayerInst || !PlayersService) {
            std::cerr << "[ClientReplicator] LocalPlayer: cant resolve Player NetId "
                       << NetId << " or game.Players isnt a real Players object\n";
            break;
        }

        PlayersService->SetLocalPlayer(LocalPlayerInst);
        std::cout << "[ClientReplicator] LocalPlayer set: " << LocalPlayerInst->GetName() << "\n";
        break;
    }
    case PacketSignal::InstanceAdded: {
        uint32_t NetId       = D.ReadUInt32();
        uint32_t ParentNetId = D.ReadUInt32();
        std::string ClassName = D.ReadString();
        std::string Name      = D.ReadString();
        ApplyInstanceAdded(NetId, ParentNetId, ClassName, Name);
        break;
    }
    case PacketSignal::InstanceRemoved: {
        uint32_t NetId = D.ReadUInt32();
        auto Inst = Instance::FindByNetId(NetId);
        if (Inst) Inst->Destroy();
        break;
    }
    case PacketSignal::InstanceChanged: {
        uint32_t NetId = D.ReadUInt32();
        std::string PropName = D.ReadString();
        auto Value = LuaArgSerializer::DeserializeArg(D, KakaScheduler::Get().GetLua());

        auto Inst = Instance::FindByNetId(NetId);
        if (!Inst) {
            std::cerr << "[ClientReplicator] InstanceChanged for unknown NetId " << NetId << "\n";
            break;
        }

        // Name aint a Reflection property, same deal as on da server side
        if (PropName == "Name") {
            if (Value.is<std::string>()) Inst->SetName(Value.as<std::string>());
            break;
        }

        std::string ClassName = Inst->GetClassName();
        const PropertyDescriptor* Prop = nullptr;
        while (!ClassName.empty()) {
            const ClassDescriptor* Desc = ClassRegistry::Get().Find(ClassName);
            if (!Desc) break;
            Prop = Desc->FindProp(PropName);
            if (Prop) break;
            ClassName = Desc->baseClassName;
        }
        if (Prop && Prop->setLua) Prop->setLua(Inst.get(), Value);
        break;
    }
    case PacketSignal::RemoteEvent: {
        uint32_t netId = D.ReadUInt32();
        auto args = LuaArgSerializer::DeserializeArgs(D, KakaScheduler::Get().GetLua());
        NetworkEvent::DispatchFire(netId, From, args, /*IsServerSide=*/false);
        break;
    }
    case PacketSignal::RemoteFunction: {
        uint8_t subtype = D.ReadByte();
        uint32_t netId  = D.ReadUInt32();
        uint64_t reqId  = D.ReadUInt64();
        auto args = LuaArgSerializer::DeserializeArgs(D, KakaScheduler::Get().GetLua());
        if (subtype == 0)
            NetworkEvent::DispatchInvokeRequest(netId, From, reqId, args, /*IsServerSide=*/false);
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

void ClientReplicator::OnConnected() {
    std::cout << "[ClientReplicator] Connected to server\n";
    if (m_log) m_log->Info("Connected to server");
    if (OnConnectedToServer) OnConnectedToServer();
}

void ClientReplicator::OnDisconnected(const std::string& Reason) {
    std::cout << "[ClientReplicator] Disconnected: " << Reason << "\n";
    if (m_log) m_log->Warn("Disconnected", Reason);
    if (OnDisconnectedFromServer) OnDisconnectedFromServer(Reason);
}