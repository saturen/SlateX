/*
    SlateX - 2026
*/
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstring>
#include "../Runtime/Engine.hpp"
#include "../Runtime/LuaVM.hpp"
#include "../Engine/AppSettings.h"
#include "../Engine/HttpUtil/HttpUtil.h"
#include "../Engine/Scripting/KakaScheduler.hpp"
#include "../Engine/JoinScript/JoinScriptLoader.h"
#include "../Network/Replicator/ServerReplicator.hpp"
#include "../Network/Replicator/ClientReplicator.hpp"

static std::atomic<bool> g_running { true };

static void SignalHandler(int) {
    g_running = false;
}

static std::string ParseStringArg(int argc, char** argv, const char* Flag, const std::string& Default) {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], Flag) == 0) return argv[i + 1];
    return Default;
}

static uint16_t ParsePortArg(int argc, char** argv, uint16_t Default) {
    for (int i = 1; i < argc - 1; i++)
        if (std::strcmp(argv[i], "-p") == 0) return static_cast<uint16_t>(std::atoi(argv[i + 1]));
    return Default;
}

int main(int argc, char** argv) {
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "[Client] SlateX Client\n";

    // -j <путь до join-эндпоинта>   -p <порт>
    std::string JoinEndpoint = ParseStringArg(argc, argv, "-j", JoinScriptLoader::kJoinEndpoint);
    uint16_t    Port         = ParsePortArg(argc, argv, 1818);

    std::cout << "[Client] -j " << JoinEndpoint << "  -p " << Port << "\n";

    AppSettings::Get().Load();
    HttpUtil::Get().Init();
    LuaVM::Get().Init();

    auto& Engine_ = Engine::Get();

    Engine_.OnClientReplicatorAwake = [](ClientReplicator& Replicator) {
        Replicator.OnConnectedToServer = []() {
            std::cout << "[Client] Connected to server!\n";
        };
        Replicator.OnDisconnectedFromServer = [](const std::string& Reason) {
            std::cout << "[Client] Disconnected: " << Reason << "\n";
            g_running = false;
        };
    };

    // лазейка в деле: если join-скрипт тайно дёрнул HostServer() вместо
    // ConnectServer() — этот коллбек сработает ВМЕСТО OnClientReplicatorAwake
    Engine_.OnServerReplicatorAwake = [](ServerReplicator& Replicator) {
        std::cout << "[Client] (psst) join script secretly hosted instead\n";
    };

    // -j/-p только задают эндпоинт и порт, решает join-скрипт
    if (!JoinScriptLoader::LoadJoin(Port, JoinEndpoint)) {
        std::cerr << "[Client] Failed to load join script — exiting\n";
        return 1;
    }

    std::cout << "[Client] Running. Ctrl+C to stop.\n";

    auto Start = std::chrono::steady_clock::now();
    while (g_running) {
        double Now = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
        Engine_.Poll();
        KakaScheduler::Get().Tick(Now);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[Client] Shutting down\n";
    Engine_.Shutdown();
    return 0;
}