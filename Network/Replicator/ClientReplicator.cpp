/*
    SlateX - 2026
*/
#include "ClientReplicator.hpp"
#include <iostream>

ClientReplicator::ClientReplicator(std::unique_ptr<ITransport> Transport)
    : ReplicatorBase(std::move(Transport)) {
    m_log = slDBG::NewLog("Logs/ClientReplicator.log", "ClientReplicator");
}

bool ClientReplicator::Connect(const std::string& Host, uint16_t Port) {
    if (m_log) m_log->Info("Connecting to", Host + ":" + std::to_string(Port));
    return m_transport->Connect(Host, Port);
}

void ClientReplicator::SendMessage(const std::string& Message) {
    Serializer S;
    S.WriteString(Message);
    m_transport->Send(PacketSignal::RemoteEvent, S);
}

void ClientReplicator::OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) {
    switch (Signal) {
    case PacketSignal::Welcome: {
        std::string Msg = D.ReadString();
        std::cout << "[ClientReplicator] Welcome from server: " << Msg << "\n";
        if (m_log) m_log->Info("Welcome", Msg);
        if (OnMessageReceived) OnMessageReceived(Msg);
        break;
    }
    case PacketSignal::RemoteEvent: {
        std::string Msg = D.ReadString();
        std::cout << "[ClientReplicator] Message from server: " << Msg << "\n";
        if (m_log) m_log->Info("Message from server", Msg);
        if (OnMessageReceived) OnMessageReceived(Msg);
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

    // da first thing we do — say hello
    SendMessage("hello from client");
}

void ClientReplicator::OnDisconnected(const std::string& Reason) {
    std::cout << "[ClientReplicator] Disconnected: " << Reason << "\n";
    if (m_log) m_log->Warn("Disconnected", Reason);
    if (OnDisconnectedFromServer) OnDisconnectedFromServer(Reason);
}