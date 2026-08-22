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
#include "debug_ostream.h"
#include <DirectXMath.h>
#include <algorithm>
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

	// Push the loaded map's point lights into the lighting pipeline. If the
	// map has more than LIGHT_MAX_POINT_LIGHTS entries, keep the closest N to
	// `origin` and warn — the rest are dropped, not silently mis-limited by
	// the shader's array bound.
	//
	// Called once at load with the map origin, then re-called every frame
	// from Map_UpdatePointLightsNearCamera with the active camera position
	// so moving through a large level picks up local lights without a full
	// reload.
	void Map_UploadPointLights(const DirectX::XMFLOAT3& origin) {
		const auto& mapLights = g_LoadedMap.lights;
		std::vector<const mapio::MapLight*> candidates;
		candidates.reserve(mapLights.size());
		for (const auto& l : mapLights) {
			if (l.type == mapio::LIGHT_POINT) candidates.push_back(&l);
		}

		// Sort by squared distance from `origin` so the "most relevant" lights
		// survive the cap.  The choice of origin differs per call site:
		//   - load time  : {0,0,0}  (no camera yet — pick anything stable)
		//   - per-frame  : the active camera position
		std::sort(candidates.begin(), candidates.end(),
			[&](const mapio::MapLight* a, const mapio::MapLight* b) {
				const float da = (a->pos[0] - origin.x) * (a->pos[0] - origin.x)
				              + (a->pos[1] - origin.y) * (a->pos[1] - origin.y)
				              + (a->pos[2] - origin.z) * (a->pos[2] - origin.z);
				const float db = (b->pos[0] - origin.x) * (b->pos[0] - origin.x)
				              + (b->pos[1] - origin.y) * (b->pos[1] - origin.y)
				              + (b->pos[2] - origin.z) * (b->pos[2] - origin.z);
				return da < db;
			});

		const int total = static_cast<int>(candidates.size());
		const int kept  = total < LIGHT_MAX_POINT_LIGHTS ? total : LIGHT_MAX_POINT_LIGHTS;
		if (total > kept) {
			hal::dout << "Map_UploadPointLights() : map has " << total
			          << " point lights, keeping closest " << kept
			          << " to origin; " << (total - kept) << " dropped" << std::endl;
		}

		Light_SetPointLightCount(kept);
		for (int i = 0; i < kept; i++) {
			const mapio::MapLight* l = candidates[i];
			const XMFLOAT3 pos{ l->pos[0], l->pos[1], l->pos[2] };
			const XMFLOAT3 col{ l->color[0], l->color[1], l->color[2] };
			Light_SetPointLightWorldByCount(i, pos, l->intensity, col);
		}
	}
}

bool Map_LoadFromFile(const char* path) {
	g_LoadedMap = mapio::MapData{};
	bool ok = mapio::Read(path, g_LoadedMap);
	g_MapLoaded = true;   // mark loaded even on failure so EnsureLoaded doesn't overwrite
	if (ok) Map_UploadPointLights({ 0.0f, 0.0f, 0.0f });
	return ok;
}

uint32_t Map_GetCollisionChecksum() {
	EnsureLoaded();
	return mapio::CollisionChecksum(g_LoadedMap);
}

DirectX::XMFLOAT3 Map_GetAmbient() {
	EnsureLoaded();
	return DirectX::XMFLOAT3{ g_LoadedMap.env.ambient[0], g_LoadedMap.env.ambient[1], g_LoadedMap.env.ambient[2] };
}

const char* Map_GetSkyAsset() {
	EnsureLoaded();
	// MapEnv::skyAsset is fixed-size char[64]; return a pointer into the
	// loaded map. The EditorMap path converts to an empty std::string; here
	// we hand back the raw char[] so an empty entry reads as "".
	return g_LoadedMap.env.skyAsset;
}

DirectX::XMFLOAT3 Map_GetFogColor() {
	EnsureLoaded();
	return DirectX::XMFLOAT3{ g_LoadedMap.env.fogColor[0], g_LoadedMap.env.fogColor[1], g_LoadedMap.env.fogColor[2] };
}

float Map_GetFogStart() {
	EnsureLoaded();
	return g_LoadedMap.env.fogStart;
}

float Map_GetFogEnd() {
	EnsureLoaded();
	return g_LoadedMap.env.fogEnd;
}

void Map_UpdatePointLightsNearCamera(const DirectX::XMFLOAT3& cameraPos) {
	Map_UploadPointLights(cameraPos);
}

bool Map_GetDirectionalLight(DirectX::XMFLOAT3* outDir, DirectX::XMFLOAT3* outColor) {
	EnsureLoaded();
	for (const auto& l : g_LoadedMap.lights) {
		if (l.type != mapio::LIGHT_DIRECTIONAL) continue;
		if (outDir)   *outDir   = { l.dir[0], l.dir[1], l.dir[2] };
		// Colour is premultiplied by intensity here so the shader remains
		// a pure multiplier; .map authors see "intensity 2.0 warm-white" as
		// one knob, not two.
		if (outColor) *outColor = { l.color[0] * l.intensity,
		                            l.color[1] * l.intensity,
		                            l.color[2] * l.intensity };
		return true;
	}
	return false;
}

bool Map_HasEnvironment() {
	EnsureLoaded();
	// A legacy default.map ships with visualSize == 0, which means its
	// MapEnv block is all zeros. Rather than silently returning pure-black
	// ambient and no sky — a visible regression vs the historical hardcoded
	// look — treat "all zeros" as "no env authored". Once every shipped map
	// carries an env block this can become a plain header-flags read.
	const auto& e = g_LoadedMap.env;
	const bool skyEmpty  = e.skyAsset[0] == '\0';
	const bool ambZero   = (e.ambient[0] == 0.0f && e.ambient[1] == 0.0f && e.ambient[2] == 0.0f);
	const bool fogZero   = (e.fogColor[0] == 0.0f && e.fogColor[1] == 0.0f && e.fogColor[2] == 0.0f);
	const bool rangeZero = (e.fogStart == 0.0f && e.fogEnd == 0.0f);
	return !(skyEmpty && ambZero && fogZero && rangeZero);
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

	// Push map lights into the lighting pipeline. Directional lights are
	// intentionally ignored for now — they will be folded into the
	// LightEnvironment once the MapEnv path lands in a later rework step.
	Map_UploadPointLights({ 0.0f, 0.0f, 0.0f });
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
