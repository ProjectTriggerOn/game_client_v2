#include "collision_world.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

void CollisionWorld::Clear()
{
	m_Colliders.clear();
}

void CollisionWorld::AddAABB(const AABB& aabb, bool isGround)
{
	m_Colliders.push_back({ aabb, isGround });
}

//-----------------------------------------------------------------------------
// Helper: Find closest point on line segment to a point
//-----------------------------------------------------------------------------
static XMFLOAT3 ClosestPointOnSegmentF3(const XMFLOAT3& segA, const XMFLOAT3& segB, const XMFLOAT3& point)
{
	float abx = segB.x - segA.x;
	float aby = segB.y - segA.y;
	float abz = segB.z - segA.z;

	float abLenSq = abx * abx + aby * aby + abz * abz;
	if (abLenSq < 1e-8f)
		return segA;

	float apx = point.x - segA.x;
	float apy = point.y - segA.y;
	float apz = point.z - segA.z;

	float t = (apx * abx + apy * aby + apz * abz) / abLenSq;
	t = std::clamp(t, 0.0f, 1.0f);

	return { segA.x + abx * t, segA.y + aby * t, segA.z + abz * t };
}

//-----------------------------------------------------------------------------
// Helper: Clamp point to AABB (closest point on AABB surface/interior)
//-----------------------------------------------------------------------------
static XMFLOAT3 ClampToAABB(const XMFLOAT3& point, const AABB& aabb)
{
	return {
		std::clamp(point.x, aabb.min.x, aabb.max.x),
		std::clamp(point.y, aabb.min.y, aabb.max.y),
		std::clamp(point.z, aabb.min.z, aabb.max.z)
	};
}

//-----------------------------------------------------------------------------
// ResolveCapsule - Detect and resolve capsule vs all AABBs
//
// Algorithm per AABB:
//   1. Build capsule segment from bottom position
//   2. Find closest point on segment to AABB
//   3. Clamp that to AABB to get closest point on AABB
//   4. If distance < radius, compute penetration and push out
//   5. If push direction is mostly upward (Y > 0.7), mark grounded
//-----------------------------------------------------------------------------
CollisionWorld::Result CollisionWorld::ResolveCapsule(
	const XMFLOAT3& capsuleBottom,
	float capsuleHeight,
	float capsuleRadius,
	const XMFLOAT3& velocity) const
{
	Result result;
	result.position = capsuleBottom;
	result.velocity = velocity;
	result.isGrounded = false;

	// Capsule segment: pointA = bottom + radius (sphere center at bottom)
	// pointB = bottom + height - radius (sphere center at top)
	// This way the capsule exactly spans from bottom to bottom+height
	float innerBottom = capsuleRadius;
	float innerTop = capsuleHeight - capsuleRadius;
	if (innerTop < innerBottom) innerTop = innerBottom; // degenerate: sphere

	// Iterate multiple times to handle corner cases where one push
	// causes overlap with another collider
	for (int iter = 0; iter < 4; ++iter)
	{
		bool anyCollision = false;

		for (const auto& collider : m_Colliders)
		{
			const AABB& aabb = collider.aabb;

			// Build capsule segment endpoints from current position
			XMFLOAT3 segA = {
				result.position.x,
				result.position.y + innerBottom,
				result.position.z
			};
			XMFLOAT3 segB = {
				result.position.x,
				result.position.y + innerTop,
				result.position.z
			};

			// Expand AABB by capsule radius (Minkowski sum approach)
			AABB expanded = {
				{ aabb.min.x - capsuleRadius, aabb.min.y - capsuleRadius, aabb.min.z - capsuleRadius },
				{ aabb.max.x + capsuleRadius, aabb.max.y + capsuleRadius, aabb.max.z + capsuleRadius }
			};

			// Find closest point on segment to expanded AABB center region
			// Actually: check if segment intersects expanded AABB
			// Simpler approach: find closest point on segment to AABB,
			// then check distance

			// Find closest point on capsule segment to the AABB
			// We need the closest point pair between segment and AABB
			XMFLOAT3 closestOnSeg = ClosestPointOnSegmentF3(segA, segB,
				{ std::clamp(segA.x, aabb.min.x, aabb.max.x),
				  std::clamp(segA.y, aabb.min.y, aabb.max.y),
				  std::clamp(segA.z, aabb.min.z, aabb.max.z) });

			// Now refine: find the point on AABB closest to this segment point
			XMFLOAT3 closestOnAABB = ClampToAABB(closestOnSeg, aabb);

			// Iterate once more for better accuracy
			closestOnSeg = ClosestPointOnSegmentF3(segA, segB, closestOnAABB);
			closestOnAABB = ClampToAABB(closestOnSeg, aabb);

			// Distance between closest points
			float dx = closestOnSeg.x - closestOnAABB.x;
			float dy = closestOnSeg.y - closestOnAABB.y;
			float dz = closestOnSeg.z - closestOnAABB.z;
			float distSq = dx * dx + dy * dy + dz * dz;

			if (distSq > capsuleRadius * capsuleRadius)
				continue; // no collision

			// Collision detected - compute push direction
			float dist = sqrtf(distSq);
			float nx, ny, nz;

			if (dist > 1e-6f)
			{
				// Normal: from AABB surface toward capsule segment
				nx = dx / dist;
				ny = dy / dist;
				nz = dz / dist;
			}
			else
			{
				// Capsule center is inside AABB - use AABB face push
				// Find minimum penetration axis
				XMFLOAT3 aabbCenter = {
					(aabb.min.x + aabb.max.x) * 0.5f,
					(aabb.min.y + aabb.max.y) * 0.5f,
					(aabb.min.z + aabb.max.z) * 0.5f
				};
				XMFLOAT3 halfSize = {
					(aabb.max.x - aabb.min.x) * 0.5f,
					(aabb.max.y - aabb.min.y) * 0.5f,
					(aabb.max.z - aabb.min.z) * 0.5f
				};

				float relX = closestOnSeg.x - aabbCenter.x;
				float relY = closestOnSeg.y - aabbCenter.y;
				float relZ = closestOnSeg.z - aabbCenter.z;

				float overlapX = halfSize.x + capsuleRadius - fabsf(relX);
				float overlapY = halfSize.y + capsuleRadius - fabsf(relY);
				float overlapZ = halfSize.z + capsuleRadius - fabsf(relZ);

				nx = ny = nz = 0.0f;
				if (overlapX <= overlapY && overlapX <= overlapZ)
					nx = (relX >= 0.0f) ? 1.0f : -1.0f;
				else if (overlapY <= overlapX && overlapY <= overlapZ)
					ny = (relY >= 0.0f) ? 1.0f : -1.0f;
				else
					nz = (relZ >= 0.0f) ? 1.0f : -1.0f;
			}

			// Penetration depth
			float penetration = capsuleRadius - dist;

			// Push capsule out along normal
			result.position.x += nx * penetration;
			result.position.y += ny * penetration;
			result.position.z += nz * penetration;

			// Zero velocity along collision normal
			float velDotN = result.velocity.x * nx + result.velocity.y * ny + result.velocity.z * nz;
			if (velDotN < 0.0f) // only if moving into the surface
			{
				result.velocity.x -= velDotN * nx;
				result.velocity.y -= velDotN * ny;
				result.velocity.z -= velDotN * nz;
			}

			// Ground check: if normal points mostly upward
			if (collider.isGround && ny > 0.7f)
			{
				result.isGrounded = true;
			}

			anyCollision = true;
		}

		if (!anyCollision) break;
	}

	return result;
}
