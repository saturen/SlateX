/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include <unordered_map>
#include <string>
#include <functional>

// server-side replicator
// handles client connections, datamodel replication, NetworkEvent traffic
class ServerReplicator : public ReplicatorBase {
public:
    explicit ServerReplicator(std::unique_ptr<ITransport> Transport);

    // start listening — this is where the party begins
    bool Start(uint16_t Port);

    // the currently running ServerReplicator, if any — lets NetworkEvent
    // (which lives in Network, same as this class) reach a live connection
    // without Network having to depend on Runtime::Engine
    static ServerReplicator* GetActive() { return s_active; }

protected:
    void OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) override;
    void OnClientConnected(ConnId Conn) override;
    void OnClientDisconnected(ConnId Conn, const std::string& Reason) override;

private:
    static ServerReplicator* s_active;
};