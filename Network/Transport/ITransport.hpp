/*
    SlateX - 2026
*/
#pragma once
#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include "Serializer.hpp"
#include "PacketSignal.hpp"

// connection handle — opaque uint64, 0 = invalid
using ConnId = uint64_t;
static constexpr ConnId InvalidConn = 0;

// transport event callbacks — set these before calling Start/Connect
struct TransportCallbacks {
    // server: new client connected
    std::function<void(ConnId)> OnClientConnected;

    // server: client disconnected
    std::function<void(ConnId, const std::string& Reason)> OnClientDisconnected;

    // client: connected to server
    std::function<void()> OnConnected;

    // client: disconnected from server
    std::function<void(const std::string& Reason)> OnDisconnected;

    // both: packet received
    // ConnId is who sent it (server side = client conn, client side = server conn)
    std::function<void(ConnId, PacketSignal, Deserializer&)> OnPacketReceived;
};

// ITransport — abstract network layer
// dont care if its GNS, ENet, raw UDP, whatever
// replicator talks to this interface only
class ITransport {
public:
    virtual ~ITransport() = default;

    // set callbacks before starting — da required
    virtual void SetCallbacks(TransportCallbacks Callbacks) = 0;

    // --- server side ---

    // start listening on port
    // returns true if it doesnt suck
    virtual bool StartServer(uint16_t Port) = 0;

    // send packet to specific client. Reliable defaults to true — every
    // core protocol packet (Welcome/Snapshot/InstanceAdded/etc) needs to
    // actually arrive, only NetworkEvent's RemoteEvent path (when
    // NetProto::UDP is set) explicitly passes false
    virtual void SendTo(ConnId Conn, PacketSignal Signal, Serializer& Data, bool Reliable = true) = 0;

    // send to all connected clients
    virtual void SendToAll(PacketSignal Signal, Serializer& Data, bool Reliable = true) = 0;

    // kick a client with reason
    virtual void Kick(ConnId Conn, const std::string& Reason) = 0;

    // get all connected client ids
    virtual std::vector<ConnId> GetClients() const = 0;

    // --- client side ---

    // connect to server
    // returns true if connection attempt started (not necessarily connected yet)
    virtual bool Connect(const std::string& Host, uint16_t Port) = 0;

    // send packet to server
    virtual void Send(PacketSignal Signal, Serializer& Data, bool Reliable = true) = 0;

    // --- both sides ---

    // poll for incoming packets — call this every frame
    virtual void Poll() = 0;

    // shut it all down
    virtual void Shutdown() = 0;

    virtual bool IsRunning() const = 0;
};