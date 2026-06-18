/*
    SlateX - 2026
*/
#pragma once
#include <memory>
#include "../Transport/ITransport.hpp"
#include "../Transport/Serializer.hpp"
#include "../Transport/PacketSignal.hpp"
#include "../../Engine/slDBG/slDBG.hpp"

// base class for server and client replicators
// handles transport wiring, subclasses handle logic
class ReplicatorBase {
public:
    explicit ReplicatorBase(std::unique_ptr<ITransport> Transport);
    virtual ~ReplicatorBase() = default;

    // call every frame
    void Poll() { m_transport->Poll(); }

    void Shutdown() { m_transport->Shutdown(); }

    ITransport* GetTransport() const { return m_transport.get(); }

protected:
    std::unique_ptr<ITransport> m_transport;
    LogFile*                    m_log = nullptr;

    // subclasses handle incoming packets
    virtual void OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) = 0;

    // subclasses handle connection events
    virtual void OnClientConnected(ConnId Conn) {}
    virtual void OnClientDisconnected(ConnId Conn, const std::string& Reason) {}
    virtual void OnConnected() {}
    virtual void OnDisconnected(const std::string& Reason) {}

    void WireCallbacks();
};