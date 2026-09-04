#include "raycast.h"
#include <cmath>

using namespace DirectX;

namespace
{
	constexpr float MAX_RANGE = 200.0f; // keep in sync with mock_server.cpp RayAABB
}

bool Raycast_AABB(const Ray& ray, const AABB& box, float& outT, XMFLOAT3& outNormal)
{
	float tMin = 0.0f;
	float tMax = MAX_RANGE;
	int axisMin = 0; // which axis produced the winning tMin
	float signMin = 1.0f;

	const float o[3] = { ray.origin.x, ray.origin.y, ray.origin.z };
	const float d[3] = { ray.direction.x, ray.direction.y, ray.direction.z };
	const float lo[3] = { box.min.x, box.min.y, box.min.z };
	const float hi[3] = { box.max.x, box.max.y, box.max.z };

	for (int i = 0; i < 3; i++)
	{
		if (std::fabs(d[i]) < 1e-8f)
		{
			if (o[i] < lo[i] || o[i] > hi[i]) return false; // parallel & outside slab
			continue;
		}
		const float inv = 1.0f / d[i];
		float t1 = (lo[i] - o[i]) * inv;
		float t2 = (hi[i] - o[i]) * inv;
		float s = -1.0f; // t1 is the lower slab -> face looks opposite dir
		if (t1 > t2)
		{
			const float tmp = t1;
			t1 = t2;
			t2 = tmp;
			s = 1.0f;
		}
		if (t1 > tMin) { tMin = t1; axisMin = i; signMin = s; }
		if (t2 < tMax) tMax = t2;
		if (tMin > tMax) return false;
	}

	// tMin==0 means the origin already lies inside: the "entry" is the origin.
	if (tMin <= 0.0f) { outT = 0.0f; return true; }

	outT = tMin;
	outNormal = { 0.0f, 0.0f, 0.0f };
	outNormal.x = (axisMin == 0) ? signMin : 0.0f;
	outNormal.y = (axisMin == 1) ? signMin : 0.0f;
	outNormal.z = (axisMin == 2) ? signMin : 0.0f;
	return true;
}
