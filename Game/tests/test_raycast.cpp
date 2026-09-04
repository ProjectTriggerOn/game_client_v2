//=============================================================================
// test_raycast.cpp — standalone ray-AABB test. NOT in the vcxproj.
// Build+run from the repo root in a vcvars64 shell:
//   cl /nologo /std:c++17 /EHsc /W4 /I . /I Game /I Core ^
//      Game\tests\test_raycast.cpp Game\raycast.cpp ^
//      /Fe:_test_raycast.exe && _test_raycast.exe
//=============================================================================
#include "../../Game/raycast.h"
#include <cstdio>
#include <cmath>

static int g_fail = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL: %s\n", m); ++g_fail; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

using namespace DirectX;

int main()
{
	// Unit box centered on origin: [-1,+1]^3
	AABB box;
	box.min = { -1.0f, -1.0f, -1.0f };
	box.max = { 1.0f,  1.0f,  1.0f };

	//-------------------------------------------------------------------------
	// 1. Six faces: hit t and outward normal
	//-------------------------------------------------------------------------
	{
		// +X face at x=1, shot from x=+5 toward -X
		Ray r{ { 5.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex +x hits");
		CHECK(Near(t, 4.0f), "hex +x t=4");
		CHECK(Near(n.x, 1.0f) && Near(n.y, 0.0f) && Near(n.z, 0.0f), "hex +x normal +X");
	}
	{
		Ray r{ { -5.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex -x hits");
		CHECK(Near(t, 4.0f), "hex -x t=4");
		CHECK(Near(n.x, -1.0f), "hex -x normal -X");
	}
	{
		Ray r{ { 0.0f, 6.0f, 0.0f }, { 0.0f, -1.0f, 0.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex +y hits");
		CHECK(Near(t, 5.0f), "hex +y t=5");
		CHECK(Near(n.y, 1.0f), "hex +y normal +Y");
	}
	{
		Ray r{ { 0.0f, -6.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex -y hits");
		CHECK(Near(n.y, -1.0f), "hex -y normal -Y");
	}
	{
		Ray r{ { 0.0f, 0.0f, -8.0f }, { 0.0f, 0.0f, 1.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex -z hits");
		CHECK(Near(t, 7.0f), "hex -z t=7");
		CHECK(Near(n.z, -1.0f), "hex -z normal -Z");
	}
	{
		Ray r{ { 0.0f, 0.0f, 8.0f }, { 0.0f, 0.0f, -1.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "hex +z hits");
		CHECK(Near(n.z, 1.0f), "hex +z normal +Z");
	}

	//-------------------------------------------------------------------------
	// 2. Miss: parallel ray outside the slab interval
	//-------------------------------------------------------------------------
	{
		Ray r{ { 5.0f, 5.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }; // parallel to +Z, x outside
		float t; XMFLOAT3 n;
		CHECK(!Raycast_AABB(r, box, t, n), "parallel miss rejects");
	}
	//-------------------------------------------------------------------------
	// 3. Start inside: t == 0 (tmin clamps to origin), still a hit
	//-------------------------------------------------------------------------
	{
		Ray r{ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, box, t, n), "inside-box shot hits");
		CHECK(Near(t, 0.0f), "inside-box t clamps to 0");
	}
	//-------------------------------------------------------------------------
	// 4. Behind the ray: box entirely in -dir -> no hit (tmax < tmin=0 start)
	//-------------------------------------------------------------------------
	{
		Ray r{ { 5.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } }; // box is behind
		float t; XMFLOAT3 n;
		CHECK(!Raycast_AABB(r, box, t, n), "box behind ray rejects");
	}
	//-------------------------------------------------------------------------
	// 5. MAX_RANGE: a far box beyond 200 units is not reported
	//-------------------------------------------------------------------------
	{
		AABB farBox; farBox.min = { 90.0f, -1.0f, -1.0f }; farBox.max = { 92.0f, 1.0f, 1.0f };
		Ray r{ { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
		float t; XMFLOAT3 n;
		CHECK(Raycast_AABB(r, farBox, t, n), "in-range far box hits");
		CHECK(Near(t, 90.0f), "far box t=90");

		AABB beyondBox; beyondBox.min = { 250.0f, -1.0f, -1.0f }; beyondBox.max = { 252.0f, 1.0f, 1.0f };
		CHECK(!Raycast_AABB(r, beyondBox, t, n), "beyond MAX_RANGE rejects");
	}

	std::printf(g_fail ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fail);
	return g_fail ? 1 : 0;
}
