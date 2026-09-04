#include <cmath>

#include "decal.h"

// Renderer include is optional: the standalone test build (Game\tests\test_decal.cpp,
// compiled without billboard.cpp) stays D3D-free. The MSBuild project defines nothing
// here, so the vcxproj build still pulls in the billboard/texture renderers for
// Decal_Draw.
#ifndef DECAL_TEST_BUILD
#include "billboard.h"
#include "texture.h"
#endif

using namespace DirectX;

namespace
{
	constexpr float DECAL_SIZE = 0.15f;  // square side, world units
	constexpr float DECAL_LIFT_WALL = 0.05f;   // walls: cube visual faces sit a hair outside the collider
	constexpr float DECAL_LIFT_FLAT = 0.015f;  // floor/ceiling: mesh lies exactly on the collider plane

	struct Decal
	{
		XMFLOAT4X4 world{}; // surface-aligned orientation + lifted position
		bool active = false;
	};

	Decal g_Decals[DECAL_MAX]{};
	int g_DecalCursor = 0; // next slot to overwrite (ring)
	int g_DecalCount = 0;

	int g_BulletHoleTexId = -1;

	// Stable tangent basis: normal is the principal axis; pick any helper not
	// parallel to it, then cross twice. Same helper choice per normal is stable.
	void BasisFromNormal(const XMFLOAT3& normal, XMFLOAT4X4& outWorld,
		const XMFLOAT3& hitPos)
	{
		const XMVECTOR n = XMLoadFloat3(&normal);

		// |n.y|<0.999 -> the normal is near-horizontal (a WALL / vertical surface):
		// world-up (0,1,0) is perpendicular to it and makes a clean reference.
		// Else the normal is near-vertical (FLOOR/CEILING), where world-up is
		// parallel to the normal, so reference world +X instead.
		XMVECTOR helper = (std::fabs(normal.y) < 0.999f)
			? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // wall: world-up reference
			: XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);  // floor/ceiling: world +X reference
		const XMVECTOR t1 = XMVector3Normalize(XMVector3Cross(helper, n));
		const XMVECTOR t2 = XMVector3Cross(n, t1);

		// Axes go into the matrix ROWS. Live-game evidence (2026-09-04): with
		// column packing the FLOOR decal rendered vertical (its local +X mapped
		// to world +Y), which is only possible if the pipeline's effective
		// transform reads basis axes from matrix ROWS. Row packing puts t1/t2/n
		// in the rows; paired with CULL_NONE in ImpactFx_Draw (decals are opaque
		// splats, double-sided rendering removes winding from the equation).
		const XMMATRIX w = XMMatrixSet(
			XMVectorGetX(t1), XMVectorGetY(t1), XMVectorGetZ(t1), 0.0f,
			XMVectorGetX(t2), XMVectorGetY(t2), XMVectorGetZ(t2), 0.0f,
			XMVectorGetX(n), XMVectorGetY(n), XMVectorGetZ(n), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		// Fold the decal size into the basis so the unit quad spans DECAL_SIZE
		// world units (rows are right/up/normal, so uniform scaling works).
		const XMMATRIX scaled = w * XMMatrixScaling(DECAL_SIZE, DECAL_SIZE, DECAL_SIZE);

		// Per-surface lift: the wall cubes' visual faces sit a hair outside the
		// collider AABBs (needs ~0.05), while the floor mesh lies exactly on the
		// collider top (0.05 floats visibly — 0.015 hides the seam).
		const float liftAmount = (std::fabs(normal.y) > 0.999f) ? DECAL_LIFT_FLAT : DECAL_LIFT_WALL;
		const XMMATRIX lift = XMMatrixTranslation(
			hitPos.x + normal.x * liftAmount,
			hitPos.y + normal.y * liftAmount,
			hitPos.z + normal.z * liftAmount);

		XMStoreFloat4x4(&outWorld, scaled * lift);
	}
}

void Decal_Initialize()
{
#ifdef DECAL_TEST_BUILD
	// Standalone test build: no renderer, no texture to load.
	g_BulletHoleTexId = -1;
#else
	g_BulletHoleTexId = Texture_LoadFromFile(L"resource/texture/bullet_hole.png");
#endif
	for (Decal& d : g_Decals) d.active = false;
	g_DecalCursor = 0;
	g_DecalCount = 0;
}

void Decal_Finalize()
{
	for (Decal& d : g_Decals) d.active = false;
	g_DecalCursor = 0;
	g_DecalCount = 0;
}

void Decal_Create(const XMFLOAT3& hitPos, const XMFLOAT3& normal)
{
	XMFLOAT4X4 world;
	BasisFromNormal(normal, world, hitPos);

	Decal& d = g_Decals[g_DecalCursor];
	d.world = world;
	d.active = true;

	g_DecalCursor = (g_DecalCursor + 1) % DECAL_MAX;
	if (g_DecalCount < DECAL_MAX) ++g_DecalCount;
}

void Decal_Draw()
{
#ifdef DECAL_TEST_BUILD
	// Standalone test build: no renderer, nothing to draw.
	return;
#else
	if (g_BulletHoleTexId < 0) return;
	const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
	for (const Decal& d : g_Decals)
	{
		if (!d.active) continue;
		const XMMATRIX w = XMLoadFloat4x4(&d.world);
		Billboard_DrawWorld(g_BulletHoleTexId, w, white);
	}
#endif
}

int Decal_GetCount()
{
	return g_DecalCount;
}

XMFLOAT4X4 Decal_DebugGetWorldMatrix(int slotIndex)
{
	return g_Decals[slotIndex].world;
}
