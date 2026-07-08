//=============================================================================
// map.cpp
//
// Map system implementation.
// Loads a runtime .map (map_io.h): box brushes -> cube draws, AABBs -> physics.
//=============================================================================

// map_io.h uses std::fopen (portable C I/O shared with the Linux server);
// silence MSVC's fopen_s deprecation (C4996, promoted to error by /sdl).
#define _CRT_SECURE_NO_WARNINGS

#include "map.h"
#include "map_io.h"
#include "collision_world.h"
#include "cube.h"
#include "mesh_field.h"
#include "texture.h"
#include "shader_3d.h"
#include "light.h"
#include "player_cam_fps.h"
#include <DirectXMath.h>
#include <vector>
#include <cstring>

using namespace DirectX;

namespace {
	struct BoxInstance { XMFLOAT3 position; XMFLOAT3 scale; };
	std::vector<BoxInstance> g_Boxes;

	int            g_CubeTexId = -1;
	mapio::MapData g_LoadedMap;
	bool           g_MapLoaded = false;

	const char* kDefaultMapPath = "resource/maps/default.map";

	void EnsureLoaded() {
		if (g_MapLoaded) return;
		if (!mapio::Read(kDefaultMapPath, g_LoadedMap)) {
			// Leave g_LoadedMap empty; the game will show an empty floor. This
			// path indicates resource/maps/default.map is missing next to the exe.
		}
		g_MapLoaded = true;
	}
}

bool Map_LoadFromFile(const char* path) {
	g_LoadedMap = mapio::MapData{};
	bool ok = mapio::Read(path, g_LoadedMap);
	g_MapLoaded = true;   // mark loaded even on failure so EnsureLoaded doesn't overwrite
	return ok;
}

uint32_t Map_GetCollisionChecksum() {
	EnsureLoaded();
	return mapio::CollisionChecksum(g_LoadedMap);
}

//-----------------------------------------------------------------------------
// Initialize — build box-brush draw instances from the loaded map's visual data
//-----------------------------------------------------------------------------
void Map_Initialize() {
	g_CubeTexId = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");
	EnsureLoaded();

	g_Boxes.clear();
	for (const auto& m : g_LoadedMap.models) {
		// Box brushes only in this phase; FBX props ("asset" != "__box__")
		// are placed by the editor plans (M2+).
		if (std::strncmp(m.asset, "__box__", sizeof("__box__")) != 0) continue;
		g_Boxes.push_back({ { m.pos[0], m.pos[1], m.pos[2] },
		                    { m.scale[0], m.scale[1], m.scale[2] } });
	}
}

//-----------------------------------------------------------------------------
// Finalize
//-----------------------------------------------------------------------------
void Map_Finalize() {
	g_Boxes.clear();
}

//-----------------------------------------------------------------------------
// Draw — ground + all box brushes (scale honored; unit boxes match the old grid)
//-----------------------------------------------------------------------------
void Map_Draw() {
	XMMATRIX mtxW = XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	MeshField_Draw(mtxW);

	for (const auto& b : g_Boxes) {
		mtxW = XMMatrixScaling(b.scale.x, b.scale.y, b.scale.z)
		     * XMMatrixTranslation(b.position.x, b.position.y, b.position.z);
		Cube_Draw(g_CubeTexId, mtxW);
	}
}

//-----------------------------------------------------------------------------
// RegisterColliders — push the loaded map's AABBs into the collision world
//-----------------------------------------------------------------------------
void Map_RegisterColliders(CollisionWorld& world) {
	EnsureLoaded();
	world.Clear();
	for (const auto& a : g_LoadedMap.colliders) {
		AABB aabb = { { a.minX, a.minY, a.minZ }, { a.maxX, a.maxY, a.maxZ } };
		world.AddAABB(aabb, a.isGround != 0);
	}
}
