/*
    SlateX - 2026
*/
#pragma once
#include <cstdint>

// packet type codes — first byte of every packet
enum class PacketSignal : uint8_t {
    // --- connection lifecycle ---
    Welcome          = 0x01,  // server -> client: string message
    Kick             = 0x02,  // server -> client: string reason

    // --- datamodel replication ---
    Snapshot         = 0x10,  // full datamodel snapshot
    SnapshotFinished = 0x11,  // snapshot done, game.Loaded fires
    InstanceAdded    = 0x12,  // new instance added to tree
    InstanceRemoved  = 0x13,  // instance removed from tree
    InstanceChanged  = 0x14,  // property changed on instance

    // --- scripting ---
    RemoteEvent      = 0x20,  // NetworkEvent fired client<->server
    RemoteFunction   = 0x21,  // da invoke with return value

    // --- misc ---
    Ping             = 0x30,
    Pong             = 0x31,
    LocalPlayer      = 0x32,  // server -> da connecting client only: NetId of their own Player
};