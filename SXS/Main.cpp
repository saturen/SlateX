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

    std::cout << "[SXS] SlateX Server\n";

    // -h <path host-endpoint>   -p <port>
    std::string HostEndpoint = ParseStringArg(argc, argv, "-h", JoinScriptLoader::kHostEndpoint);
    uint16_t    Port         = ParsePortArg(argc, argv, 1818);

    std::cout << "[SXS] -h " << HostEndpoint << "  -p " << Port << "\n";

    AppSettings::Get().Load();
    HttpUtil::Get().Init();
    LuaVM::Get().Init();

    auto& Engine_ = Engine::Get();

    Engine_.OnServerReplicatorAwake = [](ServerReplicator& Replicator) {
    };

    Engine_.OnClientReplicatorAwake = [](ClientReplicator& Replicator) {
        std::cout << "????? \n";
        Replicator.OnConnectedToServer = []() {
            std::cout << "im not going to talk about it.\n";
        };
    };

    if (!JoinScriptLoader::LoadHost(Port, HostEndpoint)) {
        std::cerr << "failed to load host\n";
        return 1;
    }

    std::cout << "[SXS] Running. Ctrl+C to stop.\n";

    auto Start = std::chrono::steady_clock::now();
    while (g_running) {
        double Now = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
        Engine_.Poll();
        KakaScheduler::Get().Tick(Now);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[SXS] Shutting down\n";
    Engine_.Shutdown();
    return 0;
}