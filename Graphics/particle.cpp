#include "particle.h"

// Renderer include is optional: the standalone test build (Game\tests\test_particle.cpp,
// compiled without billboard.cpp) stays D3D-free. The MSBuild project defines nothing
// here, so the vcxproj build still pulls in the billboard renderer for Particle_Draw.
#ifndef PARTICLE_TEST_BUILD
#include "billboard.h"
#endif

#include <cmath>
#include <cstdlib>

using namespace DirectX;

namespace
{
	struct Particle
	{
		XMFLOAT3 position{};
		XMFLOAT3 velocity{};
		XMFLOAT4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // base color (color.w = base alpha)
		float age = 0.0f;
		float lifeTime = 1.0f;
		float size = 1.0f;
		float sizeGrowRate = 0.0f;
		float gravity = 0.0f;
		float damping = 0.0f;
		bool alphaFadeout = true;
		bool active = false;
		int configId = -1;
	};

	Particle g_Particles[PARTICLE_MAX]{};
	ParticleConfig g_Configs[PARTICLE_CONFIG_MAX]{};
	bool g_ConfigUsed[PARTICLE_CONFIG_MAX]{};
	int g_NextConfigId = 0; // monotonic allocation cursor

	float RandomFloat(float min, float max)
	{
		return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX) / (max - min));
	}
}

ParticleConfig ParticleConfigDefaults()
{
	ParticleConfig c{};
	c.direction = { 0.0f, 1.0f, 0.0f };
	c.spreadDeg = 0.0f;
	c.speedMin = c.speedMax = 1.0f;
	c.lifeMin = c.lifeMax = 1.0f;
	c.sizeMin = c.sizeMax = 1.0f;
	c.sizeGrowRate = 0.0f;
	c.gravity = 0.0f;
	c.damping = 0.0f;
	c.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	c.alphaFadeout = true;
	c.textureId = -1;
	return c;
}

void Particle_Initialize()
{
	for (Particle& p : g_Particles)
	{
		p.active = false;
		p.configId = -1;
	}
	for (bool& used : g_ConfigUsed)
		used = false;
}

void Particle_Finalize()
{
	Particle_Initialize();
}

int Particle_RegisterConfig(const ParticleConfig& config)
{
	for (int i = 0; i < PARTICLE_CONFIG_MAX; ++i)
	{
		if (g_ConfigUsed[i]) continue;
		g_ConfigUsed[i] = true;
		g_Configs[i] = config;
		return i;
	}
	return -1;
}

// Uniformly tilt `direction` by a disc sample within `spreadDeg` half-angle.
static XMFLOAT3 ConeDirection(const ParticleConfig& cfg)
{
	if (cfg.spreadDeg <= 0.0f) return cfg.direction;

	const float angle = RandomFloat(0.0f, XM_2PI);           // disc azimuth
	const float radius = std::sqrt(RandomFloat(0.0f, 1.0f)); // uniform disc
	const float tilt = XMConvertToRadians(cfg.spreadDeg) * radius;

	// Build a tangent frame around the axis.
	const XMVECTOR axis = XMLoadFloat3(&cfg.direction);
	XMVECTOR helper = (std::fabs(XMVectorGetY(axis)) < 0.999f)
		? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const XMVECTOR t1 = XMVector3Normalize(XMVector3Cross(axis, helper));
	const XMVECTOR t2 = XMVector3Cross(axis, t1);

	const XMVECTOR dir = XMVector3Normalize(
		axis * std::cos(tilt)
		+ t1 * (std::sin(tilt) * std::cos(angle))
		+ t2 * (std::sin(tilt) * std::sin(angle)));

	XMFLOAT3 out;
	XMStoreFloat3(&out, dir);
	return out;
}

void Particle_Emit(int configId, const XMFLOAT3& pos, int count)
{
	if (configId < 0 || configId >= PARTICLE_CONFIG_MAX || !g_ConfigUsed[configId]) return;
	const ParticleConfig& cfg = g_Configs[configId];
	int emitted = 0;

	for (Particle& p : g_Particles)
	{
		if (emitted >= count) break;
		if (p.active) continue;

		const XMFLOAT3 dir = ConeDirection(cfg);
		const float speed = RandomFloat(cfg.speedMin, cfg.speedMax);
		const float life = RandomFloat(cfg.lifeMin, cfg.lifeMax);
		const float size = RandomFloat(cfg.sizeMin, cfg.sizeMax);

		p.position = pos;
		p.velocity = { dir.x * speed, dir.y * speed, dir.z * speed };
		p.color = cfg.color;
		p.age = 0.0f;
		p.lifeTime = life;
		p.size = size;
		p.sizeGrowRate = cfg.sizeGrowRate;
		p.gravity = cfg.gravity;
		p.damping = cfg.damping;
		p.alphaFadeout = cfg.alphaFadeout;
		p.active = true;
		p.configId = configId;
		++emitted;
	}
}

void Particle_Update(double elapsed_time)
{
	const float dt = static_cast<float>(elapsed_time);

	for (Particle& p : g_Particles)
	{
		if (!p.active) continue;

		p.age += dt;
		if (p.age >= p.lifeTime)
		{
			p.active = false;
			p.configId = -1;
			continue;
		}

		// Gravity is integrated as a leapfrog kick-drift-kick (half the impulse
		// before the position step, half after). A single step from rest therefore
		// falls exactly g*dt^2/2, matching the analytic expectation in the unit
		// test (semi-implicit Euler would overshoot to g*dt^2).
		p.velocity.y -= 0.5f * p.gravity * dt;
		const float damp = (p.damping > 0.0f) ? 1.0f - p.damping * dt : 1.0f;
		p.velocity.x *= damp;
		p.velocity.y *= damp;
		p.velocity.z *= damp;

		p.position.x += p.velocity.x * dt;
		p.position.y += p.velocity.y * dt;
		p.position.z += p.velocity.z * dt;
		p.velocity.y -= 0.5f * p.gravity * dt;

		p.size += p.sizeGrowRate * dt;
		if (p.size < 0.0f) p.size = 0.0f;

		if (p.alphaFadeout)
		{
			const float lifeRatio = p.age / p.lifeTime;
			p.color.w = 1.0f - lifeRatio;
		}
	}
}

void Particle_Draw()
{
#ifdef PARTICLE_TEST_BUILD
	// Standalone test build: no renderer, nothing to draw.
	return;
#else
	for (const Particle& p : g_Particles)
	{
		if (!p.active) continue;
		const ParticleConfig& cfg = g_Configs[p.configId];
		Billboard_Draw(cfg.textureId, p.position, { p.size, p.size }, { 0.0f, 0.0f }, p.color);
	}
#endif
}

int Particle_GetActiveCount()
{
	int n = 0;
	for (const Particle& p : g_Particles)
		if (p.active) ++n;
	return n;
}

XMFLOAT3 Particle_DebugGetPosition(int slotIndex)
{
	return g_Particles[slotIndex].position;
}

XMFLOAT4 Particle_DebugGetColor(int slotIndex)
{
	return g_Particles[slotIndex].color;
}

XMFLOAT3 Particle_DebugGetVelocity(int slotIndex)
{
	return g_Particles[slotIndex].velocity;
}
