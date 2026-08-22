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

// Lighting environment accessors. Ambient is the map's ambient light color;
// SkyAsset is the FBX path of the sky dome model (may be empty if the map
// does not author one — callers fall back to a default).
// Call after Map_Initialize so the lazy default-map load has run.
DirectX::XMFLOAT3 Map_GetAmbient();
const char*       Map_GetSkyAsset();

// Fog.  The fog "enabled" flag isn't in the wire format — it's derived on
// load from fogEnd > fogStart (> is the on/off test).  When fogStart == 0
// and fogEnd == 0 the env has no fog authored; treat that as disabled and
// Light_SetFog()'s `enabled` flag becomes redundant with the cbuffer data.
DirectX::XMFLOAT3 Map_GetFogColor();
float             Map_GetFogStart();
float             Map_GetFogEnd();

// Returns true if the loaded map authored a non-empty environment block.
// Currently visualSize==0 on default.map (the legacy map ships no env), so
// callers use this to fall back to the hardcoded in-code defaults. Remove
// once every shipped map carries an env block.
bool Map_HasEnvironment();
