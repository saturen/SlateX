/*
    SlateX - 2026
*/
#pragma once
#include <string>
#include <cstdint>
#include <functional>
#include <memory>

class ServerReplicator;
class ClientReplicator;

// Engine — единая точка входа для SXS и Client.
// Раньше каждый Main.cpp руками собирал Transport+Replicator сам —
// теперь это делает один класс, и оба процесса дёргают его одинаково.
//
// HostServer()      -> поднимает ServerReplicator, зовёт OnServerReplicatorAwake
// ConnectToServer()  -> поднимает ClientReplicator, зовёт OnClientReplicatorAwake
class Engine {
public:
    static Engine& Get();

    // запускает сервер
    bool HostServer(uint16_t Port);

    // подключается к серверу как клиент
    //
    // SecretlyHostToo — лазейка ради фана: даже находясь в роли клиента,
    // процесс ВТИХУЮ поднимает у себя ещё и сервер (как раньше делали
    // вручную через client-as-host трюк на легаси-вебсервере). Снаружи
    // это обычный клиент, но если зайти на SecretHostPort — он тоже хост.
    //
    // SecretHostPort == 0 значит "Port + 1" (чтобы не словить конфликт
    // с портом, на который сам клиент в этот момент подключается)
    bool ConnectToServer(const std::string& Address, uint16_t Port,
                         bool SecretlyHostToo = false, uint16_t SecretHostPort = 0);

    void Poll();
    void Shutdown();

    // дёргается ровно один раз — сразу после успешного старта/коннекта
    std::function<void(ServerReplicator&)> OnServerReplicatorAwake;
    std::function<void(ClientReplicator&)> OnClientReplicatorAwake;

private:
    Engine() = default;

    void InitDataModel();
    bool StartServerInternal(uint16_t Port);

    std::unique_ptr<ServerReplicator> m_server;
    std::unique_ptr<ClientReplicator> m_client;
};