#pragma once
//=============================================================================
// editor_map.h
//
// EditorMap — the editor's mutable, in-memory level model. Distinct from the
// on-disk mapio::MapData wire type (map_io.h): EditorMap uses DirectXMath and
// std::string for ergonomic editing; MapData is flat POD for the .map file.
//
// Box brushes (EditorMap::boxes) and colliders (EditorMap::colliders) are
// INDEPENDENT lists — default.map has 144 visual boxes but only 5 coarse
// colliders. See the M2 plan's "Design refinements" for the rationale.
//=============================================================================
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>
#include "map_io.h"

namespace editor {

struct MapMeta {
    std::string name;
    std::string author;
};

// A parametric scalable cube. Mirrors mapio::MapModelRef (asset == "__box__").
struct PlacedBox {
    DirectX::XMFLOAT3 pos      { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotEuler { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale    { 1.0f, 1.0f, 1.0f };   // full extents
    uint32_t          textureId = 0;
};

// An FBX prop reference. Mirrors mapio::MapModelRef (asset != "__box__").
struct PlacedModel {
    std::string       asset;                            // FBX resource path/name
    DirectX::XMFLOAT3 pos      { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 rotEuler { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale    { 1.0f, 1.0f, 1.0f };
    uint32_t          textureId = 0;
};

// An axis-aligned collision box. ownerModel is an editor-session-only link and
// is NOT persisted in v1 (the wire MapAABB has no owner field).
struct EditorCollider {
    DirectX::XMFLOAT3 min { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 max { 0.0f, 0.0f, 0.0f };
    bool              isGround   = false;
    int               ownerModel = -1;                 // index into models, or -1
};

struct SpawnPoint {
    DirectX::XMFLOAT3 pos  { 0.0f, 0.0f, 0.0f };
    float             yaw  = 0.0f;
    uint8_t           team = mapio::TEAM_RED;
};

struct EditorLight {
    uint8_t           type      = mapio::LIGHT_DIRECTIONAL;
    DirectX::XMFLOAT3 pos       { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 dir       { 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT3 color     { 1.0f, 1.0f, 1.0f };
    float             intensity = 1.0f;
};

struct EnvironmentSettings {
    std::string       skyAsset;
    DirectX::XMFLOAT3 ambient  { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 fogColor { 0.0f, 0.0f, 0.0f };
    float             fogStart = 0.0f;
    float             fogEnd   = 0.0f;
};

struct EditorMap {
    MapMeta                     meta;
    std::vector<PlacedBox>      boxes;
    std::vector<PlacedModel>    models;
    std::vector<EditorCollider> colliders;
    std::vector<SpawnPoint>     spawns;
    std::vector<EditorLight>    lights;
    EnvironmentSettings         env;
};

// Conversion to/from the on-disk wire type (editor_map.cpp).
mapio::MapData EditorMap_ToMapData(const EditorMap& m);
EditorMap      EditorMap_FromMapData(const mapio::MapData& d);

// File IO: thin wrappers over mapio::Read / mapio::Write. Return false on
// file-open/read/write failure.
bool EditorMap_Load(const char* path, EditorMap& out);
bool EditorMap_Save(const char* path, const EditorMap& m);

} // namespace editor
