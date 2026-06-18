/*
    SlateX - 2026
*/
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "../Network/Transport/GNSTransport.hpp"
#include "../Network/Replicator/ClientReplicator.hpp"

static std::atomic<bool> g_running { true };

static void SignalHandler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "[Client] SlateX Client\n";

    auto Transport  = std::make_unique<GNSTransport>();
    auto Replicator = std::make_unique<ClientReplicator>(std::move(Transport));

    Replicator->OnConnectedToServer = []() {
        std::cout << "[Client] Connected to server!\n";
        // client says hello automatically in OnConnected
    };

    Replicator->OnMessageReceived = [](const std::string& Msg) {
        std::cout << "[Client] Server says: " << Msg << "\n";
    };

    Replicator->OnDisconnectedFromServer = [](const std::string& Reason) {
        std::cout << "[Client] Disconnected: " << Reason << "\n";
        g_running = false;
    };

    if (!Replicator->Connect("127.0.0.1", 1818)) {
        std::cerr << "[Client] Failed to connect\n";
        return 1;
    }

    std::cout << "[Client] Running. Ctrl+C to stop.\n";

    while (g_running) {
        Replicator->Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[Client] Shutting down\n";
    Replicator->Shutdown();
    return 0;
}