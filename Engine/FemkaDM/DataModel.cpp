/*
    SlateX - 2026
*/
#include "DataModel.hpp"
#include <iostream>

// сервисы, которые должны существовать сразу под "game",
// даже до того как туда что-то нагрузил host/join скрипт
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
        auto svc = std::make_shared<Instance>(svcName);
        svc->SetName(svcName);
        svc->SetParent(root);
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