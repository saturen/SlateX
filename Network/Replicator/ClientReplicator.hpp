/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include "../../Engine/FemkaDM/Instance.hpp"
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

    // server says "this NetId/ParentNetId/ClassName/Name now exists" —
    // either reuse an already-local top-level service (da 6 default ones
    // CreateDefaultServices already made on this side too) or spawn a
    // fresh Instance via ClassRegistry and parent it. Used by both
    // Snapshot (looped over every entry) and InstanceAdded (one at a time)
    void ApplyInstanceAdded(uint32_t NetId, uint32_t ParentNetId,
                             const std::string& ClassName, const std::string& Name);
};