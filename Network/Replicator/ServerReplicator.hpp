/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include <unordered_map>
#include <string>
#include <functional>

// server-side replicator
// handles client connections and sends responses
class ServerReplicator : public ReplicatorBase {
public:
    explicit ServerReplicator(std::unique_ptr<ITransport> Transport);

    // start listening — this is where the party begins
    bool Start(uint16_t Port);

    // called when client sends us a message
    // set this from your server script
    std::function<void(ConnId, const std::string&)> OnMessageReceived;

    // send a message to specific client
    void SendMessage(ConnId Conn, const std::string& Message);

    // send to all connected clients
    void Broadcast(const std::string& Message);

protected:
    void OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) override;
    void OnClientConnected(ConnId Conn) override;
    void OnClientDisconnected(ConnId Conn, const std::string& Reason) override;
};