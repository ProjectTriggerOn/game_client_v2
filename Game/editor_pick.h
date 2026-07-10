#pragma once
//=============================================================================
// editor_pick.h — screen<->world math for the editor (spec §3.3, §8.2).
// Reuses collision.h Ray/AABB. Left-handed, row-vector matrices (DirectXMath).
//=============================================================================
#include <DirectXMath.h>
#include "collision.h"

// Unproject a pixel (sx,sy; origin top-left) into a world-space ray.
Ray  EditorPick_ScreenRay(int sx, int sy, const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

// Slab test. On hit, tHit is the ray parameter of the nearest entry (>=0).
bool EditorPick_RayAABB(const Ray& r, const AABB& box, float& tHit);

// Ray vs. infinite plane (point + normal). false if parallel / behind origin.
bool EditorPick_RayPlane(const Ray& r, const DirectX::XMFLOAT3& planePt,
                         const DirectX::XMFLOAT3& planeN, DirectX::XMFLOAT3& hit);

// Project a world point to pixel coords. false if behind the camera.
bool EditorPick_WorldToScreen(const DirectX::XMFLOAT3& w, const DirectX::XMFLOAT4X4& view,
                              const DirectX::XMFLOAT4X4& proj, DirectX::XMFLOAT2& screen);

// Intersect with the horizontal plane y == planeY. false if the ray is parallel / points away.
bool EditorPick_RayGroundY(const Ray& r, float planeY, DirectX::XMFLOAT3& hit);
