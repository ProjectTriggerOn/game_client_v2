#pragma once
//=============================================================================
// net_packet.h
//
// Packet type identifiers for network serialization.
// Shared between game_client and game_server (maintain in sync).
//=============================================================================

#include <cstdint>

enum class PacketType : uint8_t
{
    INPUT_CMD = 1,   // Client -> Server (contains InputCmd)
    SNAPSHOT  = 2,   // Server -> Client (contains Snapshot)
};
