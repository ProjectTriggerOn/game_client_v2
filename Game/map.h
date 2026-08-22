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
namespace mapio { struct MapData; }

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

// Directional light.  If the map authored at least one LIGHT_DIRECTIONAL
// entry, returns its direction and color into outDir / outColor and
// returns true.  If no directional light is authored the outputs are
// untouched and the function returns false; the caller should fall back
// to its own default.
//
// Only the first LIGHT_DIRECTIONAL record is consumed.  Authoring more
// than one is permitted by the format but ignored here; revisit if
// multi-directional support is ever needed.
bool Map_GetDirectionalLight(DirectX::XMFLOAT3* outDir,
                             DirectX::XMFLOAT3* outColor);

// Re-sort map point lights by distance to `cameraPos`, then re-upload the
// closest LIGHT_MAX_POINT_LIGHTS into the lighting pipeline.  Call once per
// frame after Camera_SetMatrixToShader so eye_pos is valid; the upload is
// dirty-flagged (no GPU traffic when camera didn't move enough to change
// the top-16).
void Map_UpdatePointLightsNearCamera(const DirectX::XMFLOAT3& cameraPos);

// Editor hook: push a fresh MapData (typically produced by EditorMap_ToMapData
// in the scene editor) into the module's loaded-map slot so that the runtime
// accessors below (Map_GetAmbient, Map_GetSkyAsset, etc.) observe it
// immediately.  Does NOT touch the .map file on disk.
void Map_SetLoadedData(const mapio::MapData& d);

// Returns true if the loaded map authored a non-empty environment block.
// Currently visualSize==0 on default.map (the legacy map ships no env), so
// callers use this to fall back to the hardcoded in-code defaults. Remove
// once every shipped map carries an env block.
bool Map_HasEnvironment();
