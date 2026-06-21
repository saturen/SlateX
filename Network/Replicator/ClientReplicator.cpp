/*
    SlateX - 2026
*/
#include "ClientReplicator.hpp"
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
    // RemoteEvent / RemoteFunction are handled by NetworkEvent (see
    // Network/Shared/NetworkEvent.hpp)
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