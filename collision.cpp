#include "collision.h"
#include <DirectXMath.h>
using namespace DirectX;

bool Collision_OverlapCircleCircle(const Circle& a, const Circle& b)
{
	float x1 = b.center.x - a.center.x;
	float y1 = b.center.y - a.center.y;

	return (a.radius + b.radius) * (a.radius + b.radius) > (x1 * x1 + y1 * y1);

	//XMVECTOR centerA = XMLoadFloat2(&a.center);
	//XMVECTOR centerB = XMLoadFloat2(&b.center);
	//XMVECTOR lsq = XMVector2LengthSq(centerB - centerA);

	//return (a.radius + b.radius) * (a.radius + b.radius) > XMVectorGetX(lsq);
}

bool Collision_OverlapCircleBox(const Box& a, const Box& b)
{
	float at = a.center.y - a.halfHeight;
	float ab = a.center.y + a.halfHeight;
	float al = a.center.x - a.halfWidth;
	float ar = a.center.x + a.halfWidth;
	float bt = b.center.y - b.halfHeight;
	float bb = b.center.y + b.halfHeight;
	float bl = b.center.x - b.halfWidth;
	float br = b.center.x + b.halfWidth;

	return al < br && ar > bl && at < bb && ab > bt;
}
