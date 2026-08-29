#pragma once
//=============================================================================
// raycast.h — ray/AABB slab intersection with an outward face normal.
// Pure math, no engine dependencies (unit-testable standalone).
// Semantics follow mock_server.cpp RayAABB: MAX_RANGE 200, entry parameter
// only (inside-box hits report t=0); normal is one of the six axis faces.
//=============================================================================
#include <DirectXMath.h>
#include "collision.h"

// On hit: returns true, outT = ray parameter of the entry (0..200), outNormal
// = unit outward normal of the entry face. On miss: returns false, outT/outN
// untouched.
bool Raycast_AABB(const Ray& ray, const AABB& box, float& outT,
	DirectX::XMFLOAT3& outNormal);
