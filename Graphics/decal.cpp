#include <cmath>

#include "decal.h"
#include "billboard.h"
#include "texture.h"

using namespace DirectX;

namespace
{
	constexpr float DECAL_SIZE = 0.15f;  // square side, world units
	constexpr float DECAL_LIFT = 0.01f;  // along the normal, anti z-fighting

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

		XMVECTOR helper = (std::fabs(normal.y) < 0.999f)
			? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // floor/ceiling: reference up in world
			: XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);  // vertical wall: world +X
		const XMVECTOR t1 = XMVector3Normalize(XMVector3Cross(helper, n));
		const XMVECTOR t2 = XMVector3Cross(n, t1);

		// Billboard shader multiplies posL (x,y in quad plane, z unused) by world:
		// rows are the quad's right (t1), up (t2) and normal (n).
		const XMMATRIX w = XMMatrixSet(
			XMVectorGetX(t1), XMVectorGetX(t2), XMVectorGetX(n), 0.0f,
			XMVectorGetY(t1), XMVectorGetY(t2), XMVectorGetY(n), 0.0f,
			XMVectorGetZ(t1), XMVectorGetZ(t2), XMVectorGetZ(n), 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		// Fold the decal size into the basis so the unit quad spans DECAL_SIZE
		// world units (rows are right/up/normal, so uniform scaling works).
		const XMMATRIX scaled = w * XMMatrixScaling(DECAL_SIZE, DECAL_SIZE, DECAL_SIZE);

		const XMMATRIX lift = XMMatrixTranslation(
			hitPos.x + normal.x * DECAL_LIFT,
			hitPos.y + normal.y * DECAL_LIFT,
			hitPos.z + normal.z * DECAL_LIFT);

		XMStoreFloat4x4(&outWorld, scaled * lift);
	}
}

void Decal_Initialize()
{
	g_BulletHoleTexId = Texture_LoadFromFile(L"resource/texture/bullet_hole.png");
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
	if (g_BulletHoleTexId < 0) return;
	const XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
	for (const Decal& d : g_Decals)
	{
		if (!d.active) continue;
		const XMMATRIX w = XMLoadFloat4x4(&d.world);
		Billboard_DrawWorld(g_BulletHoleTexId, w, white);
	}
}

int Decal_GetCount()
{
	return g_DecalCount;
}
