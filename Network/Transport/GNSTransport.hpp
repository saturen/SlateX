/*
    SlateX - 2026
*/
#pragma once
#include "ITransport.hpp"
#include <GameNetworkingSockets/steam/steamnetworkingsockets.h>
#include <GameNetworkingSockets/steam/steamnetworkingtypes.h>
#include <vector>
#include <unordered_map>

// GameNetworkingSockets implementation of ITransport
// reliable, encrypted, works on all platforms
// da good shit

class GNSTransport : public ITransport {
public:
    GNSTransport();
    ~GNSTransport() override;

    void SetCallbacks(TransportCallbacks Callbacks) override;

    // --- server ---
    bool StartServer(uint16_t Port) override;
    void SendTo(ConnId Conn, PacketSignal Signal, Serializer& Data) override;
    void SendToAll(PacketSignal Signal, Serializer& Data) override;
    void Kick(ConnId Conn, const std::string& Reason) override;
    std::vector<ConnId> GetClients() const override;

    // --- client ---
    bool Connect(const std::string& Host, uint16_t Port) override;
    void Send(PacketSignal Signal, Serializer& Data) override;

    // --- both ---
    void Poll() override;
    void Shutdown() override;
    bool IsRunning() const override { return m_running; }

private:
    // GNS conn handle -> our ConnId (they are the same, just cast)
    static ConnId ToConnId(HSteamNetConnection Conn) {
        return static_cast<ConnId>(Conn);
    }
    static HSteamNetConnection ToGNS(ConnId Conn) {
        return static_cast<HSteamNetConnection>(Conn);
    }

    // sends raw bytes to a GNS connection
    void SendRaw(HSteamNetConnection Conn, const std::vector<uint8_t>& Bytes);

    // dispatches incoming message to OnPacketReceived callback
    void DispatchMessage(ConnId From, const void* Data, size_t Size);

    // GNS status callbacks — static because GNS requires it
    static void ServerStatusCallback(SteamNetConnectionStatusChangedCallback_t* Info);
    static void ClientStatusCallback(SteamNetConnectionStatusChangedCallback_t* Info);

    // singleton ptr so static callbacks can reach back
    // da classic GNS pattern
    static GNSTransport* s_instance;

    ISteamNetworkingSockets* m_interface    = nullptr;
    HSteamListenSocket       m_listenSocket = k_HSteamListenSocket_Invalid;
    HSteamNetPollGroup       m_pollGroup    = k_HSteamNetPollGroup_Invalid;
    HSteamNetConnection      m_serverConn   = k_HSteamNetConnection_Invalid;

    std::vector<ConnId> m_clients;
    TransportCallbacks  m_callbacks;
    bool                m_running = false;
    bool                m_isServer = false;
};