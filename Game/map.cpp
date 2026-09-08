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
#include "net_common.h"
#include "cube.h"
#include "mesh_field.h"
#include "model.h"
#include "model_catalog.h"
#include "texture.h"
#include "shader_3d.h"
#include "light.h"
#include "player_cam_fps.h"
#include "debug_ostream.h"
#include <DirectXMath.h>
#include <algorithm>
#include <string>
#include <vector>
#include <cstring>

using namespace DirectX;

namespace {
	struct BoxInstance {
		XMFLOAT3 position;
		XMFLOAT3 scale;
		uint32_t textureId = 0;
	};

	// A placed FBX prop from the map's visual section. The MODEL* itself lives
	// in the ModelCatalog cache (keyed by asset path); this struct only holds
	// the placement + the asset name string the cache is queried with.
	struct PropInstance {
		std::string asset;
		XMFLOAT3    pos;
		XMFLOAT3    rot;    // euler radians, XMMatrixRotationRollPitchYaw order
		XMFLOAT3    scale;
	};

	std::vector<BoxInstance> g_Boxes;
	std::vector<PropInstance> g_Props;

	// Box-brush texture palette, indexed by MapModelRef::textureId. Index 0 is
	// the historical default (stone), so maps authored before the palette
	// existed render exactly as before; an out-of-range id, or a slot whose
	// file failed to load, falls back to it. See Map_Initialize for the slots.
	std::vector<int> g_BoxTexPalette;

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
			// NOTE: MapLight::intensity means different things per light type.
			// Map_GetDirectionalLight premultiplies a directional light's colour by
			// it, but here it is the point light's RANGE in metres, and the colour
			// goes through unscaled. Authoring a point light "1.0 warm white,
			// intensity 1.1" therefore asks for a full-brightness lamp with a 1.1 m
			// radius, which washes a small map out. Dim the colour itself.
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

void Map_SetLoadedData(const mapio::MapData& d) {
	g_LoadedMap = d;
	g_MapLoaded = true;
	// Re-upload points so the runtime immediately matches the new map.
	Map_UploadPointLights({ 0.0f, 0.0f, 0.0f });
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

//-----------------------------------------------------------------------------
// Authored spawn points (see map.h). Kept in map order so callers that hand
// out successive indices spread across the team's points.
//-----------------------------------------------------------------------------
// map.h documents `team` as 0 = RED / 1 = BLUE so callers need not include
// map_io.h just for two constants; pin that down rather than leave it to a
// coincidence between the wire format and the gameplay enum.
static_assert(mapio::TEAM_RED  == PlayerTeam::RED,  "team id mismatch");
static_assert(mapio::TEAM_BLUE == PlayerTeam::BLUE, "team id mismatch");

int Map_GetSpawnCount(uint8_t team) {
	EnsureLoaded();
	int n = 0;
	for (const auto& s : g_LoadedMap.spawns)
		if (s.team == team) n++;
	return n;
}

bool Map_GetSpawn(uint8_t team, int index,
                  DirectX::XMFLOAT3* outPos, float* outYaw) {
	EnsureLoaded();
	if (index < 0) return false;
	for (const auto& s : g_LoadedMap.spawns) {
		if (s.team != team) continue;
		if (index-- > 0) continue;
		if (outPos) *outPos = XMFLOAT3(s.x, s.y, s.z);
		if (outYaw) *outYaw = s.yaw;
		return true;
	}
	return false;
}

bool Map_HasEnvironment() {
	EnsureLoaded();
	// A map with no authored env carries an all-zero MapEnv block. Rather
	// than silently returning pure-black ambient and no sky — a visible
	// regression vs the historical hardcoded look — treat "all zeros" as
	// "no env authored". Once every shipped map carries an env block this
	// can become a plain header-flags read. (default.map does author an
	// env: sky + 0.5 ambient, written by tools/map_convert.cpp.)
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

	// Box-brush texture palette. Slot 0 is the historical default (stone), so
	// maps authored before the palette existed render exactly as before; an
	// out-of-range id or a slot that failed to load falls back to it.
	// Shipment draws its containers, crates and walls as FBX props, so the
	// only brush texture it needs is the yard's concrete.
	struct TexEntry { const wchar_t* path; };
	const TexEntry kPalette[] = {
		{ L"resource/texture/ground_concrete.png" },   // 1
	};
	g_BoxTexPalette.clear();
	g_BoxTexPalette.push_back(g_CubeTexId);            // 0 => stone (legacy default)
	for (const auto& t : kPalette)
		g_BoxTexPalette.push_back(Texture_LoadFromFile(t.path));

	EnsureLoaded();

	g_Boxes.clear();
	g_Props.clear();
	for (const auto& m : g_LoadedMap.models) {
		if (std::strncmp(m.asset, "__box__", sizeof("__box__")) != 0) {
			// FBX prop: record the instance; the MODEL* resolves lazily in
			// Map_Draw via ModelCatalog (first draw of an asset pays its load).
			// Failed loads stay in the list as nullptr and are skipped per
			// frame — cheap, and keeps Map_Finalize/Initialize symmetric.
			g_Props.push_back({ m.asset,
			                    { m.pos[0], m.pos[1], m.pos[2] },
			                    { m.rotEuler[0], m.rotEuler[1], m.rotEuler[2] },
			                    { m.scale[0], m.scale[1], m.scale[2] } });
			continue;
		}
		g_Boxes.push_back({ { m.pos[0], m.pos[1], m.pos[2] },
		                    { m.scale[0], m.scale[1], m.scale[2] },
		                    m.textureId });
	}

	// The catalog scans resource/model once and lazy-loads FBXs on demand —
	// the same cache the editor uses. Safe to call repeatedly; Finalize is
	// editor-owned so the runtime never releases the cache mid-session.
	ModelCatalog_Init("resource/model");

	// Push map lights into the lighting pipeline. Directional lights are
	// intentionally ignored for now — they will be folded into the
	// LightEnvironment once the MapEnv path lands in a later rework step.
	Map_UploadPointLights({ 0.0f, 0.0f, 0.0f });

	const size_t propCount = g_Props.size();
	if (propCount > 0) {
		hal::dout << "Map_Initialize() : " << propCount << " FBX props queued"
		          << std::endl;
	}
}

//-----------------------------------------------------------------------------
// Finalize
//-----------------------------------------------------------------------------
void Map_Finalize() {
	g_Boxes.clear();
	g_Props.clear();
}

//-----------------------------------------------------------------------------
// Draw — ground + box brushes + FBX props (scale honored; unit boxes match
// the old grid)
//-----------------------------------------------------------------------------
void Map_Draw() {
	// NOTE: MeshField_Draw marks its matrix parameter [[maybe_unused]] and
	// builds its own at y = 0, so this -1 never takes effect and the debug
	// grid lands on the play plane. default.map uses that grid as its floor,
	// so it is left alone; a map that authors its own ground has to raise the
	// slab clear of y = 0 or the two co-planar surfaces z-fight.
	XMMATRIX mtxW = XMMatrixTranslation(0.0f, -1.0f, 0.0f);
	MeshField_Draw(mtxW);

	for (const auto& b : g_Boxes) {
		mtxW = XMMatrixScaling(b.scale.x, b.scale.y, b.scale.z)
		     * XMMatrixTranslation(b.position.x, b.position.y, b.position.z);
		// Palette lookup with stone fallback: unknown/out-of-range ids and
		// slots that failed to load both render as the legacy default.
		int texId = g_CubeTexId;
		if (b.textureId < g_BoxTexPalette.size() && g_BoxTexPalette[b.textureId] >= 0)
			texId = g_BoxTexPalette[b.textureId];
		Cube_Draw(texId, mtxW);
	}

	// FBX props. ModelCatalog_Get caches per asset name and returns nullptr
	// for unknown/failed loads (cached, so the failure cost is one attempt) —
	// skip those silently: a propless prop slot beats a crash.
	for (const auto& p : g_Props) {
		MODEL* model = ModelCatalog_Get(p.asset.c_str());
		if (!model) continue;
		mtxW = XMMatrixScaling(p.scale.x, p.scale.y, p.scale.z)
		     * XMMatrixRotationRollPitchYaw(p.rot.x, p.rot.y, p.rot.z)
		     * XMMatrixTranslation(p.pos.x, p.pos.y, p.pos.z);
		ModelDraw(model, mtxW);
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
