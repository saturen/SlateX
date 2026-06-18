/*
    SlateX - 2026
*/
#include "GNSTransport.hpp"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <arpa/inet.h>
#include <netdb.h>

GNSTransport* GNSTransport::s_instance = nullptr;

GNSTransport::GNSTransport() {
    s_instance = this;

    SteamDatagramErrMsg ErrMsg;
    if (!GameNetworkingSockets_Init(nullptr, ErrMsg))
        throw std::runtime_error(std::string("[GNSTransport] Init failed: ") + ErrMsg);

    m_interface = SteamNetworkingSockets();
}

GNSTransport::~GNSTransport() {
    Shutdown();
    GameNetworkingSockets_Kill();
    s_instance = nullptr;
}

void GNSTransport::SetCallbacks(TransportCallbacks Callbacks) {
    m_callbacks = std::move(Callbacks);
}

// =============================================
//  Server
// =============================================

bool GNSTransport::StartServer(uint16_t Port) {
    m_isServer = true;

    SteamNetworkingIPAddr Addr;
    Addr.Clear();
    Addr.m_port = Port;

    SteamNetworkingConfigValue_t Opt;
    Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               (void*)ServerStatusCallback);

    m_listenSocket = m_interface->CreateListenSocketIP(Addr, 1, &Opt);
    if (m_listenSocket == k_HSteamListenSocket_Invalid) {
        std::cerr << "[GNSTransport] CreateListenSocketIP failed\n";
        return false;
    }

    m_pollGroup = m_interface->CreatePollGroup();
    m_running   = true;

    std::cout << "[GNSTransport] Server listening on port " << Port << "\n";
    return true;
}

void GNSTransport::SendTo(ConnId Conn, PacketSignal Signal, Serializer& Data) {
    Serializer Packet;
    Packet.WriteByte(static_cast<uint8_t>(Signal));
    Packet.WriteRawBuffer(Data.GetBuffer());
    SendRaw(ToGNS(Conn), Packet.GetBuffer());
}

void GNSTransport::SendToAll(PacketSignal Signal, Serializer& Data) {
    for (auto Conn : m_clients)
        SendTo(Conn, Signal, Data);
}

void GNSTransport::Kick(ConnId Conn, const std::string& Reason) {
    m_interface->CloseConnection(ToGNS(Conn), 0, Reason.c_str(), true);
}

std::vector<ConnId> GNSTransport::GetClients() const {
    return m_clients;
}

// =============================================
//  Client
// =============================================

bool GNSTransport::Connect(const std::string& Host, uint16_t Port) {
    m_isServer = false;

    SteamNetworkingIPAddr Addr;
    Addr.Clear();

    // Сначала пробуем как IPv4
    struct in_addr ipv4;
    if (inet_pton(AF_INET, Host.c_str(), &ipv4) == 1) {
        Addr.SetIPv4(ntohl(ipv4.s_addr), Port);
    } else {
        // Резолвим домен
        struct addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(Host.c_str(), nullptr, &hints, &res) != 0 || !res) {
            std::cerr << "[GNSTransport] failed to resolve host: " << Host << "\n";
            return false;
        }
        auto* sa = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
        Addr.SetIPv4(ntohl(sa->sin_addr.s_addr), Port);
        freeaddrinfo(res);
    }

    SteamNetworkingConfigValue_t Opt;
    Opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
               (void*)ClientStatusCallback);

    m_serverConn = m_interface->ConnectByIPAddress(Addr, 1, &Opt);
    if (m_serverConn == k_HSteamNetConnection_Invalid) {
        std::cerr << "[GNSTransport] ConnectByIPAddress failed\n";
        return false;
    }

    m_running = true;
    return true;
}

void GNSTransport::Send(PacketSignal Signal, Serializer& Data) {
    if (m_serverConn == k_HSteamNetConnection_Invalid) return;

    Serializer Packet;
    Packet.WriteByte(static_cast<uint8_t>(Signal));
    Packet.WriteRawBuffer(Data.GetBuffer());
    SendRaw(m_serverConn, Packet.GetBuffer());
}

// =============================================
//  Both
// =============================================

void GNSTransport::Poll() {
    if (!m_interface) return;
    m_interface->RunCallbacks();

    if (m_isServer && m_pollGroup != k_HSteamNetPollGroup_Invalid) {
        ISteamNetworkingMessage* Msgs[64];
        int Count = m_interface->ReceiveMessagesOnPollGroup(m_pollGroup, Msgs, 64);
        for (int i = 0; i < Count; i++) {
            DispatchMessage(ToConnId(Msgs[i]->m_conn),
                            Msgs[i]->GetData(), Msgs[i]->GetSize());
            Msgs[i]->Release();
        }
    } else if (!m_isServer && m_serverConn != k_HSteamNetConnection_Invalid) {
        ISteamNetworkingMessage* Msgs[64];
        int Count = m_interface->ReceiveMessagesOnConnection(m_serverConn, Msgs, 64);
        for (int i = 0; i < Count; i++) {
            DispatchMessage(ToConnId(Msgs[i]->m_conn),
                            Msgs[i]->GetData(), Msgs[i]->GetSize());
            Msgs[i]->Release();
        }
    }
}

void GNSTransport::Shutdown() {
    if (!m_running) return;
    m_running = false;

    for (auto Conn : m_clients)
        m_interface->CloseConnection(ToGNS(Conn), 0, nullptr, false);
    m_clients.clear();

    if (m_serverConn != k_HSteamNetConnection_Invalid) {
        m_interface->CloseConnection(m_serverConn, 0, nullptr, false);
        m_serverConn = k_HSteamNetConnection_Invalid;
    }

    if (m_listenSocket != k_HSteamListenSocket_Invalid) {
        m_interface->CloseListenSocket(m_listenSocket);
        m_listenSocket = k_HSteamListenSocket_Invalid;
    }

    if (m_pollGroup != k_HSteamNetPollGroup_Invalid) {
        m_interface->DestroyPollGroup(m_pollGroup);
        m_pollGroup = k_HSteamNetPollGroup_Invalid;
    }
}

// =============================================
//  Internal
// =============================================

void GNSTransport::SendRaw(HSteamNetConnection Conn, const std::vector<uint8_t>& Bytes) {
    m_interface->SendMessageToConnection(
        Conn, Bytes.data(), static_cast<uint32_t>(Bytes.size()),
        k_nSteamNetworkingSend_Reliable, nullptr
    );
}

void GNSTransport::DispatchMessage(ConnId From, const void* Data, size_t Size) {
    if (!m_callbacks.OnPacketReceived || Size == 0) return;

    try {
        const uint8_t* Bytes = static_cast<const uint8_t*>(Data);
        PacketSignal   Signal = static_cast<PacketSignal>(Bytes[0]);
        Deserializer   D(Bytes + 1, Size - 1);
        m_callbacks.OnPacketReceived(From, Signal, D);
    } catch (const std::exception& E) {
        std::cerr << "[GNSTransport] DispatchMessage error: " << E.what() << "\n";
    }
}

// =============================================
//  Static GNS callbacks
// =============================================

void GNSTransport::ServerStatusCallback(
        SteamNetConnectionStatusChangedCallback_t* Info) {
    if (!s_instance) return;
    auto& T = *s_instance;

    switch (Info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_Connecting:
        SteamNetworkingSockets()->AcceptConnection(Info->m_hConn);
        SteamNetworkingSockets()->SetConnectionPollGroup(
            Info->m_hConn, T.m_pollGroup);
        T.m_clients.push_back(ToConnId(Info->m_hConn));
        std::cout << "[GNSTransport] Client connecting\n";
        if (T.m_callbacks.OnClientConnected)
            T.m_callbacks.OnClientConnected(ToConnId(Info->m_hConn));
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        ConnId Conn = ToConnId(Info->m_hConn);
        std::string Reason = Info->m_info.m_szEndDebug;
        std::cout << "[GNSTransport] Client disconnected: " << Reason << "\n";
        if (T.m_callbacks.OnClientDisconnected)
            T.m_callbacks.OnClientDisconnected(Conn, Reason);
        T.m_clients.erase(
            std::remove(T.m_clients.begin(), T.m_clients.end(), Conn),
            T.m_clients.end());
        SteamNetworkingSockets()->CloseConnection(Info->m_hConn, 0, nullptr, false);
        break;
    }
    default: break;
    }
}

void GNSTransport::ClientStatusCallback(
        SteamNetConnectionStatusChangedCallback_t* Info) {
    if (!s_instance) return;
    auto& T = *s_instance;

    switch (Info->m_info.m_eState) {
    case k_ESteamNetworkingConnectionState_Connected:
        std::cout << "[GNSTransport] Connected to server\n";
        if (T.m_callbacks.OnConnected)
            T.m_callbacks.OnConnected();
        break;

    case k_ESteamNetworkingConnectionState_ClosedByPeer:
    case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
        std::string Reason = Info->m_info.m_szEndDebug;
        std::cout << "[GNSTransport] Disconnected: " << Reason << "\n";
        if (T.m_callbacks.OnDisconnected)
            T.m_callbacks.OnDisconnected(Reason);
        T.m_serverConn = k_HSteamNetConnection_Invalid;
        break;
    }
    default: break;
    }
}