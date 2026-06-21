/*
    SlateX - 2026
*/
#include "Engine.hpp"
#include "../Network/Transport/GNSTransport.hpp"
#include "../Network/Replicator/ServerReplicator.hpp"
#include "../Network/Replicator/ClientReplicator.hpp"
#include "../Engine/FemkaDM/DataModel.hpp"
#include <iostream>

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

void Engine::InitDataModel() {
    // creats datamodel
    DataModel::Get();
}

bool Engine::StartServerInternal(uint16_t Port) {
    auto transport = std::make_unique<GNSTransport>();
    m_server = std::make_unique<ServerReplicator>(std::move(transport));

    if (!m_server->Start(Port)) {
        std::cerr << "[Engine] Failed to start server on port " << Port << "\n";
        m_server.reset();
        return false;
    }

    if (OnServerReplicatorAwake) OnServerReplicatorAwake(*m_server);
    return true;
}

bool Engine::HostServer(uint16_t Port) {
    InitDataModel();
    return StartServerInternal(Port);
}

bool Engine::ConnectToServer(const std::string& Address, uint16_t Port,
                              bool SecretlyHostToo, uint16_t SecretHostPort) {
    InitDataModel();

    auto transport = std::make_unique<GNSTransport>();
    m_client = std::make_unique<ClientReplicator>(std::move(transport));

    if (!m_client->Connect(Address, Port)) {
        std::cerr << "[Engine] Failed to connect to " << Address << ":" << Port << "\n";
        m_client.reset();
        return false;
    }

    if (OnClientReplicatorAwake) OnClientReplicatorAwake(*m_client);

    // .......im not gonna comment that
    if (SecretlyHostToo) {
        uint16_t hostPort = SecretHostPort != 0 ? SecretHostPort : static_cast<uint16_t>(Port + 1);
        std::cout << "[Engine] (psst) secretly hosting too, on port " << hostPort << "\n";
        StartServerInternal(hostPort);
    }

    return true;
}

void Engine::Poll() {
    if (m_server) m_server->Poll();
    if (m_client) m_client->Poll();
}

void Engine::Shutdown() {
    if (m_server) { m_server->Shutdown(); m_server.reset(); }
    if (m_client) { m_client->Shutdown(); m_client.reset(); }
}