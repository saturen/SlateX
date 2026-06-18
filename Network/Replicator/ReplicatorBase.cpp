/*
    SlateX - 2026
*/
#include "ReplicatorBase.hpp"

ReplicatorBase::ReplicatorBase(std::unique_ptr<ITransport> Transport)
    : m_transport(std::move(Transport)) {
    WireCallbacks();
}

void ReplicatorBase::WireCallbacks() {
    TransportCallbacks Cb;

    Cb.OnClientConnected = [this](ConnId Conn) {
        OnClientConnected(Conn);
    };
    Cb.OnClientDisconnected = [this](ConnId Conn, const std::string& Reason) {
        OnClientDisconnected(Conn, Reason);
    };
    Cb.OnConnected = [this]() {
        OnConnected();
    };
    Cb.OnDisconnected = [this](const std::string& Reason) {
        OnDisconnected(Reason);
    };
    Cb.OnPacketReceived = [this](ConnId From, PacketSignal Signal, Deserializer& D) {
        OnPacketReceived(From, Signal, D);
    };

    m_transport->SetCallbacks(std::move(Cb));
}