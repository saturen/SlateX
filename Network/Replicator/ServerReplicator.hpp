/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include "../../Engine/FemkaDM/Instance.hpp"
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

    // hooks Root's own OnChildAdded (root never gets a NetId itself — its
    // children's ParentNetId in the wire format is just 0, no need for root
    // to have a real id), then walks whatever's already parented under it
    void WatchRoot(InstanceRef Root);

    // da actual workhorse — assigns a NetId, hooks OnChildAdded/OnDestroyed
    // for future changes, recurses into existing children, and (if Broadcast
    // is true) tells every connected client about it right now. Broadcast
    // is false during the initial startup walk (nobody's connected yet
    // anyway) and true for anything discovered live afterwards.
    //
    // skips the whole subtree without assigning anything if Inst's
    // FilterMode is Server — that's the point of FilterMode::Server
    void WatchAndMaybeBroadcast(InstanceRef Inst, bool Broadcast);

    // sends da full current tree to one freshly connected client
    void SendSnapshotTo(ConnId Conn);

    void BroadcastInstanceAdded(InstanceRef Inst);
    void BroadcastInstanceRemoved(InstanceRef Inst);
    void BroadcastInstanceChanged(InstanceRef Inst, const std::string& PropName);

    // simple counter, starts at 1 — 0 is reserved for "no NetId" / "root"
    uint32_t AllocateNetId() { return m_nextNetId++; }
    uint32_t m_nextNetId = 1;
};