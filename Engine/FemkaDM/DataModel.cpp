/*
    SlateX - 2026
*/
#include "DataModel.hpp"
#include "../Reflection/Reflection.hpp"
#include <iostream>

// services that gotta exist under "game" right away, before host/join
// script touches anything
static void CreateDefaultServices(InstanceRef root) {
    const char* services[] = {
        "Workspace",
        "Players",
        "ReplicatedStorage",
        "ServerScriptService",
        "ServerStorage",
        "Lighting",
    };

    for (auto* svcName : services) {
        std::string name = svcName;

        // Players needs to be a real Players object (PlayerAdded/
        // PlayerRemoving/LocalPlayer), not a bare Instance — it's
        // registered in Network/Shared/Players.cpp, not here, since
        // Engine cant depend on Network (same reason NetworkEvent lives
        // there too). Going through ClassRegistry keeps DataModel.cpp
        // from ever having to know Players exists as a concrete type.
        InstanceRef svc = name == "Players"
            ? ClassRegistry::Get().Create("Players")
            : std::make_shared<Instance>(svcName);

        if (!svc) {
            // shouldnt happen — Network/Shared/Players.cpp registers da
            // factory at static-init time, same proven pattern as
            // NetworkEvent — but better a warning than a null-deref crash
            std::cerr << "[DataModel] Players factory not registered, falling back to bare Instance\n";
            svc = std::make_shared<Instance>(svcName);
        }

        svc->SetName(svcName);
        svc->SetParent(root);

        // server-only, never leaves the server — da whole point of FilterMode::Server
        if (name == "ServerScriptService" || name == "ServerStorage")
            svc->SetFilterMode(FilterMode::Server);
    }

    std::cout << "[DataModel] Default services created\n";
}

InstanceRef DataModel::Get() {
    static InstanceRef root = [] {
        InstanceRef r = std::shared_ptr<DataModel>(new DataModel());
        CreateDefaultServices(r);
        return r;
    }();
    return root;
}