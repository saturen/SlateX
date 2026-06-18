/*
    SlateX - 2026
*/
#pragma once
#include "ReplicatorBase.hpp"
#include <functional>
#include <string>

// client-side replicator
// connects to server and exchanges messages
class ClientReplicator : public ReplicatorBase {
public:
    explicit ClientReplicator(std::unique_ptr<ITransport> Transport);

    // connect to server
    bool Connect(const std::string& Host, uint16_t Port);

    // called when server sends us a message
    std::function<void(const std::string&)> OnMessageReceived;

    // called when we connect successfully
    std::function<void()> OnConnectedToServer;

    // called when we get kicked or disconnected
    std::function<void(const std::string&)> OnDisconnectedFromServer;

    // send a message to server
    void SendMessage(const std::string& Message);

protected:
    void OnPacketReceived(ConnId From, PacketSignal Signal, Deserializer& D) override;
    void OnConnected() override;
    void OnDisconnected(const std::string& Reason) override;
};