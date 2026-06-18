/*
    SlateX - 2026
*/
#include "ServerReplicator.hpp"
#include <iostream>

ServerReplicator::ServerReplicator(std::unique_ptr<ITransport> Transport)
    : ReplicatorBase(std::move(Transport)) {
    m_log = slDBG::NewLog("Logs/ServerReplicator.log", "ServerReplicator");
}

bool ServerReplicator::Start(uint16_t Port) {
    bool Ok = m_transport->StartServer(Port);
    if (Ok && m_log)
        m_log->Info("Server started", std::to_string(Port));
    return Ok;
}

void ServerReplicator::SendMessage(ConnId Conn, const std::string& Message) {
    Serializer S;
    S.WriteString(Message);
    m_transport->SendTo(Conn, PacketSignal::RemoteEvent, S);
}

void ServerReplicator::Broadcast(const std::string& Message) {
    Serializer S;
    S.WriteString(Message);
    m_transport->SendToAll(PacketSignal::RemoteEvent, S);
}

void ServerReplicator::OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) {
    switch (Signal) {
    case PacketSignal::RemoteEvent: {
        std::string Msg = D.ReadString();
        if (m_log) m_log->Info("Message from client", Msg);
        if (OnMessageReceived)
            OnMessageReceived(From, Msg);
        break;
    }
    case PacketSignal::Ping: {
        // da polite response
        Serializer S;
        m_transport->SendTo(From, PacketSignal::Pong, S);
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
}

void ServerReplicator::OnClientDisconnected(ConnId Conn, const std::string& Reason) {
    std::cout << "[ServerReplicator] Client disconnected: " << Reason << "\n";
    if (m_log) m_log->Info("Client disconnected", Reason);
}