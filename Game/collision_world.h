#pragma once
//=============================================================================
// collision_world.h
//
// Collision world with registerable AABB colliders.
// Provides Capsule vs AABB collision detection and response for gravity.
//=============================================================================

#include <vector>
#include <DirectXMath.h>
#include "collision.h"

struct ColliderAABB
{
	AABB aabb;
	bool isGround; // true = can land on this surface
};

class CollisionWorld
{
public:
	void Clear();
	void AddAABB(const AABB& aabb, bool isGround = true);

	struct Result
	{
		DirectX::XMFLOAT3 position; // corrected position (capsule bottom)
		DirectX::XMFLOAT3 velocity; // corrected velocity
		bool isGrounded;            // true if standing on a ground surface
	};

	// Resolve capsule collision against all registered AABBs.
	// capsuleBottom: the foot position of the capsule
	// capsuleHeight: total height (pointA=bottom, pointB=bottom+height)
	// capsuleRadius: capsule radius
	// velocity: current velocity (will be zeroed on collision axes)
	Result ResolveCapsule(const DirectX::XMFLOAT3& capsuleBottom,
	                      float capsuleHeight,
	                      float capsuleRadius,
	                      const DirectX::XMFLOAT3& velocity) const;

	const std::vector<ColliderAABB>& GetColliders() const { return m_Colliders; }

private:
	std::vector<ColliderAABB> m_Colliders;
};
