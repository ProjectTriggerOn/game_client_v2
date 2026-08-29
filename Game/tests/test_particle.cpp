//=============================================================================
// test_particle.cpp — standalone test for Graphics/particle.{h,cpp} logic.
// NOT in the vcxproj. particle.cpp must stay D3D-free (DirectXMath only).
// Build+run from the repo root in a vcvars64 shell:
//   cl /nologo /std:c++17 /EHsc /W4 /DPARTICLE_TEST_BUILD /I . /I Graphics ^
//      Game\tests\test_particle.cpp Graphics\particle.cpp ^
//      /Fe:_test_particle.exe && _test_particle.exe
// (/DPARTICLE_TEST_BUILD makes particle.cpp compile without the D3D billboard
//  renderer so this TU has no link-time dependency on billboard.cpp.)
//=============================================================================
#include "../../Graphics/particle.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++g_fail; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

using namespace DirectX;

int main()
{
	//-------------------------------------------------------------------------
	// 1. Registration: ids increment, full registry rejects
	//-------------------------------------------------------------------------
	Particle_Initialize();
	for (int i = 0; i < PARTICLE_CONFIG_MAX; ++i)
		CHECK(Particle_RegisterConfig(ParticleConfigDefaults()) == i, "config ids assign sequentially");
	CHECK(Particle_RegisterConfig(ParticleConfigDefaults()) == -1, "full registry returns -1");

	Particle_Finalize();
	Particle_Initialize();

	//-------------------------------------------------------------------------
	// 2. Zero-spread zero-speed config -> static particles, expire at lifetime
	//-------------------------------------------------------------------------
	ParticleConfig cfg = ParticleConfigDefaults();
	cfg.lifeMin = cfg.lifeMax = 10.0f;   // nothing expires mid-test
	cfg.speedMin = cfg.speedMax = 0.0f;  // no motion
	cfg.gravity = 0.0f;
	cfg.spreadDeg = 0.0f;

	const int id = Particle_RegisterConfig(cfg);
	CHECK(id >= 0, "config registers");

	Particle_Emit(id, { 1.0f, 2.0f, 3.0f }, 10);
	CHECK(Particle_GetActiveCount() == 10, "emit allocates count particles");

	Particle_Update(0.1);
	CHECK(Particle_GetActiveCount() == 10, "nothing expires before lifetime");

	Particle_Update(9.95); // total age 10.05 > 10
	CHECK(Particle_GetActiveCount() == 0, "all particles expire at lifetime");

	//-------------------------------------------------------------------------
	// 3. Emit beyond capacity drops the overflow, never overwrites live ones
	//-------------------------------------------------------------------------
	Particle_Emit(id, { 0.0f, 0.0f, 0.0f }, PARTICLE_MAX + 50);
	CHECK(Particle_GetActiveCount() == PARTICLE_MAX, "pool fills to exactly PARTICLE_MAX");
	Particle_Emit(id, { 0.0f, 0.0f, 0.0f }, 5);
	CHECK(Particle_GetActiveCount() == PARTICLE_MAX, "emit into full pool drops");

	Particle_Finalize();

	//-------------------------------------------------------------------------
	// 4. Physics: velocity integration along direction + gravity on -Y
	//-------------------------------------------------------------------------
	Particle_Initialize();

	ParticleConfig c2 = ParticleConfigDefaults();
	c2.lifeMin = c2.lifeMax = 10.0f;
	c2.direction = { 0.0f, 0.0f, 1.0f }; // +Z
	c2.spreadDeg = 0.0f;                 // exact direction, no randomness
	c2.speedMin = c2.speedMax = 4.0f;    // constant speed 4
	c2.gravity = 10.0f;                  // pulls -Y
	c2.damping = 0.0f;
	c2.alphaFadeout = false;
	const int id2 = Particle_RegisterConfig(c2);
	CHECK(id2 >= 0, "c2 registers");

	Particle_Emit(id2, { 0.0f, 0.0f, 0.0f }, 1);
	Particle_Update(0.5);
	// position = dir*speed*t = (0,0,2); y = -g*t^2/2 = -1.25
	const XMFLOAT3 p = Particle_DebugGetPosition(0);
	CHECK(Near(p.z, 2.0f) && Near(p.y, -1.25f), "velocity integration + gravity");
	Particle_Finalize();

	//-------------------------------------------------------------------------
	// 5. Alpha fadeout: color.w decreases over age, other channels untouched
	//-------------------------------------------------------------------------
	Particle_Initialize();
	ParticleConfig c3 = ParticleConfigDefaults();
	c3.lifeMin = c3.lifeMax = 10.0f;
	c3.speedMin = c3.speedMax = 0.0f;
	c3.gravity = 0.0f;
	c3.alphaFadeout = true;
	c3.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	const int id3 = Particle_RegisterConfig(c3);
	CHECK(id3 >= 0, "c3 registers");

	Particle_Emit(id3, { 0.0f, 0.0f, 0.0f }, 1);
	Particle_Update(5.0); // half life
	const XMFLOAT4 col = Particle_DebugGetColor(0);
	CHECK(Near(col.w, 0.5f), "alpha at half-life is half");
	CHECK(Near(col.x, 1.0f) && Near(col.y, 1.0f) && Near(col.z, 1.0f), "rgb untouched by fade");
	Particle_Finalize();

	//-------------------------------------------------------------------------
	// 6. Cone sampling: spreadDeg=0 -> exactly direction*speed; 90deg cone from
	//    +Y never emits downward (half-angle bound)
	//-------------------------------------------------------------------------
	Particle_Initialize();
	ParticleConfig c4 = ParticleConfigDefaults();
	c4.lifeMin = c4.lifeMax = 1.0f;
	c4.direction = { 0.0f, 1.0f, 0.0f }; // +Y
	c4.spreadDeg = 90.0f;                // half-angle 90: no particle below horizon
	c4.speedMin = c4.speedMax = 1.0f;
	c4.gravity = 0.0f;
	const int id4 = Particle_RegisterConfig(c4);

	bool anyBelowHorizon = false;
	for (int burst = 0; burst < 40; ++burst)
	{
		Particle_Emit(id4, { 0.0f, 0.0f, 0.0f }, 25);
		Particle_Finalize();
		Particle_Initialize();
		const int rerun = Particle_RegisterConfig(c4);
		Particle_Emit(rerun, { 0.0f, 0.0f, 0.0f }, 25);
		for (int i = 0; i < 25; ++i)
			if (Particle_DebugGetVelocity(i).y < -1e-4f) anyBelowHorizon = true;
	}
	CHECK(!anyBelowHorizon, "cone never emits below half-angle bound");
	Particle_Finalize();

	std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
