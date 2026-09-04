#pragma once
//=============================================================================
// particle.h — data-driven particle pool (ported from LPSP particle_system +
// particle_test, reworked: no virtual emitter hierarchy, a registered
// ParticleConfig table drives randomization instead). Pure logic — D3D-free,
// unit-testable; only Particle_Draw touches the renderer.
//=============================================================================
#include <DirectXMath.h>

constexpr int PARTICLE_MAX = 1024;        // particle pool slots
constexpr int PARTICLE_CONFIG_MAX = 32;   // registered config slots

// Emission recipe. All ranged fields are [min, max] uniform samples per
// particle. Velocity direction = `direction` tilted by a uniform disc sample
// of half-angle `spreadDeg` (0 = exactly `direction`).
struct ParticleConfig
{
	DirectX::XMFLOAT3 direction{ 0.0f, 1.0f, 0.0f }; // unit vector (cone axis)
	float spreadDeg = 0.0f;      // cone half-angle in degrees
	float speedMin = 1.0f;
	float speedMax = 1.0f;
	float lifeMin = 1.0f;        // seconds
	float lifeMax = 1.0f;
	float sizeMin = 1.0f;        // world units (quad scale)
	float sizeMax = 1.0f;
	float sizeGrowRate = 0.0f;   // units per second, may be negative
	float gravity = 0.0f;        // units per second^2, pulls -Y
	float damping = 0.0f;        // velocity *= (1 - damping*dt), 0 = none
	DirectX::XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
	bool alphaFadeout = true;    // scale alpha by remaining lifetime
	int textureId = -1;          // texture used by Particle_Draw
};

ParticleConfig ParticleConfigDefaults();

void Particle_Initialize();
void Particle_Finalize();
void Particle_Update(double elapsed_time);
void Particle_Draw(); // renderer side only

int Particle_RegisterConfig(const ParticleConfig& config); // -1 when full
void Particle_Emit(int configId, const DirectX::XMFLOAT3& pos, int count);

// Overrides only the cone axis for this call; the registered config keeps
// everything else (spread, speed, life, size, color...). Used for sparks that
// must spray along a per-burst direction (e.g. the hit surface normal) that
// wasn't known at config-registration time.
void Particle_EmitDirectional(int configId, const DirectX::XMFLOAT3& pos,
	int count, const DirectX::XMFLOAT3& direction);

int Particle_GetActiveCount();

// Test/inspection accessors (index into the pool, not a handle).
DirectX::XMFLOAT3 Particle_DebugGetPosition(int slotIndex);
DirectX::XMFLOAT4 Particle_DebugGetColor(int slotIndex);
DirectX::XMFLOAT3 Particle_DebugGetVelocity(int slotIndex);
