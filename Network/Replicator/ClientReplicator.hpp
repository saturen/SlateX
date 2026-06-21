/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include <functional>
#include <string>

// client-side replicator
// connects to server, handles datamodel replication, NetworkEvent traffic
class ClientReplicator : public ReplicatorBase {
public:
    explicit ClientReplicator(std::unique_ptr<ITransport> Transport);

    // connect to server
    bool Connect(const std::string& Host, uint16_t Port);

    // called when we connect successfully
    std::function<void()> OnConnectedToServer;

    // called when we get kicked or disconnected
    std::function<void(const std::string&)> OnDisconnectedFromServer;

    // the currently running ClientReplicator, if any — lets NetworkEvent
    // (which lives in Network, same as this class) reach a live connection
    // without Network having to depend on Runtime::Engine
    static ClientReplicator* GetActive() { return s_active; }

protected:
    void OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) override;
    void OnConnected() override;
    void OnDisconnected(const std::string& Reason) override;

private:
    static ClientReplicator* s_active;
};