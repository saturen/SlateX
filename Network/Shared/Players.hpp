/*
    SlateX - 2026
*/
#pragma once
#include "../../Engine/FemkaDM/Instance.hpp"
#include "../../Engine/FemkaDM/Player.hpp"
#include "Signal.hpp"

// Players — da game.Players service. Lives in Network/Shared, not
// Engine/FemkaDM, for da same reason NetworkEvent does: needs Signal,
// and Engine aint allowed to depend on Network (see Runtime/Engine.hpp).
//
// PlayerAdded/PlayerRemoving fire on da SERVER side only (only da server
// ever creates/destroys Player instances, see ServerReplicator). Clients
// get told about Players showing up/leaving through da regular
// InstanceAdded/InstanceRemoved replication path same as anything else —
// dey CAN listen to PlayerAdded/PlayerRemoving too, it's just dat on da
// client dose Signals only fire as a side effect of replication catching
// up, not because da client itself decided anyone joined.
//
// LocalPlayer is client-only — server never sets it (nobody to set it to),
// see ClientReplicator's handling of PacketSignal::LocalPlayer.
class Players : public Instance {
public:
    Players() : Instance("Players") {}

    bool IsA(const std::string& ClassName) const override {
        if (ClassName == "Players") return true;
        return Instance::IsA(ClassName);
    }

    Signal PlayerAdded;
    Signal PlayerRemoving;

    InstanceRef GetLocalPlayer() const { return m_localPlayer; }
    void        SetLocalPlayer(InstanceRef P) { m_localPlayer = P; }

    // just da children that are actually Players — in practice dats
    // everything parented here, but filtering keeps it honest
    std::vector<std::shared_ptr<Player>> GetPlayers() const {
        std::vector<std::shared_ptr<Player>> Out;
        for (auto& Child : GetChildren()) {
            auto P = std::dynamic_pointer_cast<Player>(Child);
            if (P) Out.push_back(P);
        }
        return Out;
    }

    // registers da class+factory+push with ClassRegistry — called
    // explicitly from LuaVM::Init(), NOT a static-initializer trick.
    //
    // why dis matters: an anonymous-namespace static bool with nothing
    // else externally referenced from da same .cpp can get SILENTLY
    // DROPPED by da linker — static archives only pull in .o files dat
    // something actually references a symbol from, and Players.cpp had
    // nothing like dat (everything else here is header-inline). Learned
    // dis da hard way — "Players factory not registered" at runtime,
    // no compile/link error, no warning, just silently missing. An
    // explicit call from somewhere guaranteed to run (LuaVM::Init) cant
    // be dropped like dat.
    static void RegisterClass();

private:
    InstanceRef m_localPlayer;
};