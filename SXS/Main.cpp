/*
    SlateX - 2026
*/
#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include "../Network/Transport/GNSTransport.hpp"
#include "../Network/Replicator/ServerReplicator.hpp"

static std::atomic<bool> g_running { true };

static void SignalHandler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGINT,  SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    std::cout << "[SXS] SlateX Server\n";

    // create transport and replicator
    auto Transport   = std::make_unique<GNSTransport>();
    auto Replicator  = std::make_unique<ServerReplicator>(std::move(Transport));

    // handle incoming messages from clients
    Replicator->OnMessageReceived = [&](ConnId Conn, const std::string& Msg) {
        std::cout << "[SXS] Client " << Conn << " says: " << Msg << "\n";

        // da polite response
        Replicator->SendMessage(Conn, "hello back from server");
    };

    // start listening
    if (!Replicator->Start(1818)) {
        std::cerr << "[SXS] Failed to start server\n";
        return 1;
    }

    std::cout << "[SXS] Running. Ctrl+C to stop.\n";

    while (g_running) {
        Replicator->Poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "[SXS] Shutting down\n";
    Replicator->Shutdown();
    return 0;
}