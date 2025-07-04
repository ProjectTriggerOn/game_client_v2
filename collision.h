#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXMath.h>


struct Circle
{
	DirectX::XMFLOAT2 center; // 圆心坐标
	float radius; // 半径
};

struct Box
{
	DirectX::XMFLOAT2 center;
	float halfWidth; // 水平半宽
	float halfHeight; // 垂直半宽
};

bool Collision_OverlapCircleCircle(const Circle& a, const Circle& b);
bool Collision_OverlapCircleBox(const Box& a, const Box& b);

#endif // COLLISION_H
