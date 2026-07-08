#pragma once
//=============================================================================
// map.h
//
// Map system — generates draw objects from grid data,
// renders visual objects, and registers colliders for physics.
//=============================================================================

#include <DirectXMath.h>

// Forward declaration
class CollisionWorld;

void Map_Initialize();
void Map_Finalize();
void Map_Draw();

// Register all map colliders into a CollisionWorld
void Map_RegisterColliders(CollisionWorld& world);

// Load a .map file into the module's in-memory map. Idempotent-safe to call
// once at startup; Map_Initialize / Map_RegisterColliders lazily load
// "resource/maps/default.map" if this was never called. Returns false on error.
bool Map_LoadFromFile(const char* path);

// FNV-1a checksum over the loaded map's collision section (for the connect
// handshake). Forces a lazy load if needed.
uint32_t Map_GetCollisionChecksum();
