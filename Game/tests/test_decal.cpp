//=============================================================================
// test_decal.cpp — standalone test for Graphics/decal.{h,cpp} logic.
// NOT in the vcxproj. decal.cpp must stay D3D-free in the test build
// (/DDECAL_TEST_BUILD skips the billboard/texture renderer includes and the
//  texture load), so this TU has no link-time dependency on billboard.cpp.
// Build+run from the repo root in a vcvars64 shell:
//   cl /nologo /std:c++17 /EHsc /W4 /DDECAL_TEST_BUILD /I . /I Graphics ^
//      Game\tests\test_decal.cpp Graphics\decal.cpp ^
//      /Fe:_test_decal.exe && _test_decal.exe
//=============================================================================
#include "../../Graphics/decal.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++g_fail; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

using namespace DirectX;

int main()
{
	//-------------------------------------------------------------------------
	// 1. Ring buffer caps at DECAL_MAX: DECAL_MAX+1 creates keep count at MAX
	//-------------------------------------------------------------------------
	Decal_Initialize();
	for (int i = 0; i < DECAL_MAX + 1; ++i)
		Decal_Create({ (float)i * 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
	CHECK(Decal_GetCount() == DECAL_MAX, "count caps at DECAL_MAX");

	//-------------------------------------------------------------------------
	// 2. Ring order: oldest overwritten first. Slot 0 holds write #DECAL_MAX
	//    (translation.x = DECAL_MAX), slot 1 holds write #1 (translation.x = 1).
	//-------------------------------------------------------------------------
	const XMFLOAT4X4 slot0 = Decal_DebugGetWorldMatrix(0);
	const XMFLOAT4X4 slot1 = Decal_DebugGetWorldMatrix(1);
	CHECK(Near(slot0._41, (float)DECAL_MAX), "slot 0 overwritten by newest write");
	CHECK(Near(slot1._41, 1.0f), "slot 1 holds write #1");
	Decal_Finalize();

	//-------------------------------------------------------------------------
	// 3. Basis orientation (floor decal, normal +Y): axes packed as COLUMNS
	//    (matches the working camera path; row packing flips the quad's front
	//    face into the surface and CULL_BACK discards it).
	//    For n=(0,1,0): t1=cross(up,n)=(0,0,1), t2=cross(n,t1)=(1,0,0).
	//    Column packing, scaled by DECAL_SIZE: column0=t1*S=(0,0,0.15),
	//    column1=t2*S=(0.15,0,0), column2=n*S=(0,0.15,0) — i.e. in XMFLOAT4X4
	//    storage: _11=0,_12=0.15,_13=0 / _21=0,_22=0,_23=0.15 /
	//    _31=0.15,_32=0,_33=0. The +Y normal appears at _23=0.15.
	//-------------------------------------------------------------------------
	Decal_Initialize();
	Decal_Create({ 5.0f, 0.0f, 5.0f }, { 0.0f, 1.0f, 0.0f });
	const XMFLOAT4X4 floorWorld = Decal_DebugGetWorldMatrix(0);
	CHECK(Near(floorWorld._23, 0.15f), "normal +Y*S lands at _23 (column-packed)");
	CHECK(Near(floorWorld._12, 0.15f), "t2.x*S at _12 (t2=+X in column1)");
	CHECK(Near(floorWorld._31, 0.15f), "t1.z*S at _31 (t1=+Z in column0)");
	CHECK(Near(floorWorld._13, 0.0f) && Near(floorWorld._33, 0.0f), "normal z components zero");

	//-------------------------------------------------------------------------
	// 4. Lift: world._42 = hitPos.y + normal.y * DECAL_LIFT = 0 + 0.01
	//-------------------------------------------------------------------------
	CHECK(Near(floorWorld._42, 0.01f), "decal lifted 0.01 along +Y (anti z-fight)");
	Decal_Finalize();

	std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
