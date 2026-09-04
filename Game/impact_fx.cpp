#include "impact_fx.h"
#include "raycast.h"
#include "decal.h"
#include "particle.h"
#include "billboard.h"
#include "collision_world.h"
#include "game.h"
#include "player_fps.h"
#include "direct3d.h"
#include "texture.h"
#include "debug_log.h"

using namespace DirectX;

namespace
{
	// Spark burst settings (tweakable constants, spec §3.7).
	constexpr int   SPARKS_PER_HIT = 12;
	constexpr float SPARK_SPEED_MIN = 1.5f;
	constexpr float SPARK_SPEED_MAX = 6.0f;
	constexpr float SPARK_LIFE_MIN = 0.25f;
	constexpr float SPARK_LIFE_MAX = 0.6f;
	constexpr float SPARK_SIZE_MIN = 0.05f;
	constexpr float SPARK_SIZE_MAX = 0.12f;
	constexpr float SPARK_SPREAD_DEG = 55.0f; // cone around the surface normal
	constexpr float SPARK_GRAVITY = 9.8f;
	constexpr float SPARK_GROW = -0.05f;      // sparks shrink as they cool

	int g_SparkTexId = -1;
	int g_SparkConfigId = -1;
	int g_LastFireCounter = -1; // last seen PlayerFps fire counter
}

void ImpactFx_Initialize()
{
	// Renderer bases first (billboard owns the quad + its shader).
	Billboard_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());
	Decal_Initialize();
	Particle_Initialize();

	g_SparkTexId = Texture_LoadFromFile(L"resource/texture/sparks.png");

	// One reusable spark config; the cone axis is overridden per burst via
	// Particle_EmitDirectional (hit normal), so `direction` here is unused.
	ParticleConfig spark = ParticleConfigDefaults();
	spark.spreadDeg = SPARK_SPREAD_DEG;
	spark.speedMin = SPARK_SPEED_MIN;
	spark.speedMax = SPARK_SPEED_MAX;
	spark.lifeMin = SPARK_LIFE_MIN;
	spark.lifeMax = SPARK_LIFE_MAX;
	spark.sizeMin = SPARK_SIZE_MIN;
	spark.sizeMax = SPARK_SIZE_MAX;
	spark.sizeGrowRate = SPARK_GROW;
	spark.gravity = SPARK_GRAVITY;
	spark.damping = 1.5f;                        // sparks lose speed quickly in air
	spark.color = { 1.0f, 0.75f, 0.35f, 1.0f };  // hot orange
	spark.alphaFadeout = true;
	spark.textureId = g_SparkTexId;
	g_SparkConfigId = Particle_RegisterConfig(spark);

	g_LastFireCounter = -1;
}

void ImpactFx_Finalize()
{
	Particle_Finalize();
	Decal_Finalize();
	Billboard_Finalize();
}

// Fire one spark burst back along the hit face normal.
static void HitSparkBurst(const XMFLOAT3& hitPos, const XMFLOAT3& normal)
{
	Particle_EmitDirectional(g_SparkConfigId, hitPos, SPARKS_PER_HIT, normal);
}

void ImpactFx_Update(double elapsed_time)
{
	// Particles advance every frame regardless of the fire-counter state below,
	// so in-flight sparks keep moving even while the shot-polling early-outs.
	Particle_Update(elapsed_time);

	// g_PlayerFps lives in game.cpp's anonymous namespace, so game.h exposes
	// Game_GetLocalPlayer() for the player side and Game_GetCollisionWorld()
	// for the world side. Poll the client-predicted fire counter: it increments
	// in PlayerFps::ConsumeRound at trigger time, which is exactly when a
	// hitscan should be visualised client-side.
	PlayerFps* player = Game_GetLocalPlayer();
	if (!player) return;

	const int fire = player->GetFireCounter();
	if (g_LastFireCounter == -1) { g_LastFireCounter = fire; return; }

	// u16 wrap-aware difference: how many shots since last frame.
	const unsigned diff = static_cast<unsigned>(fire - g_LastFireCounter) & 0xFFFFu;
	if (diff == 0) return;
	g_LastFireCounter = fire;
	// Rollback repair can rewind the counter (ApplyServerCorrection); a giant
	// jump back is a resync, not a burst of shots — cap at one visual burst.
	const int shots = (diff > 8) ? 1 : static_cast<int>(diff);

	// Shot ray mirrors the server formula: eye + camera front (player_fps.h
	// GetFront is the live aim direction used for the debug ray in game.cpp).
	const XMFLOAT3 eye = player->GetEyePosition();
	const XMFLOAT3 dir = player->GetFront();

	const CollisionWorld* world = Game_GetCollisionWorld();
	if (!world) return;

	for (int s = 0; s < shots; ++s)
	{
		const Ray ray{ eye, dir };
		float bestT = 0.0f;
		XMFLOAT3 bestNormal{};
		bool hit = false;
		for (const auto& col : world->GetColliders())
		{
			float t = 0.0f;
			XMFLOAT3 n{};
			if (Raycast_AABB(ray, col.aabb, t, n) && (!hit || t < bestT))
			{
				hit = true;
				bestT = t;
				bestNormal = n;
			}
		}
		// Raycast_AABB reports origin-inside-box hits as t==0 with outNormal
		// left untouched — treat those as no visual impact (nothing to hit).
		if (!hit || bestT <= 0.0f) continue;

		const XMFLOAT3 hitPos{
			eye.x + dir.x * bestT,
			eye.y + dir.y * bestT,
			eye.z + dir.z * bestT };

		Decal_Create(hitPos, bestNormal);
		HitSparkBurst(hitPos, bestNormal);

		// TEMP DIAGNOSTIC (missing-wall-directions): log every spawn's normal +
		// lifted center so the missing orientations can be diffed offline.
		const XMFLOAT3 lifted{ hitPos.x + bestNormal.x * 0.05f,
			hitPos.y + bestNormal.y * 0.05f,
			hitPos.z + bestNormal.z * 0.05f };
		DebugLog_Printf("impact",
			"spawn n=(%.2f,%.2f,%.2f) hit=(%.2f,%.2f,%.2f) center=(%.2f,%.2f,%.2f)",
			bestNormal.x, bestNormal.y, bestNormal.z,
			hitPos.x, hitPos.y, hitPos.z, lifted.x, lifted.y, lifted.z);
	}
}

void ImpactFx_SetCamera(const XMFLOAT4X4& view)
{
	Billboard_SetCamera(view);
}

void ImpactFx_Draw()
{
	Direct3D_SetDepthWriteEnable(false);
	// TEMP DIAGNOSTIC (direction asymmetry): disable back-face culling so a
	// wrong-winding quad still renders — separates culling from transform bugs.
	Direct3D_SetCullMode(D3D11_CULL_NONE);
	Decal_Draw();
	Direct3D_SetCullMode(D3D11_CULL_BACK);
	Particle_Draw();
	Direct3D_SetDepthWriteEnable(true);
}

void ImpactFx_DebugInfo(int& decalCount, int& particleCount)
{
	decalCount = Decal_GetCount();
	particleCount = Particle_GetActiveCount();
}
