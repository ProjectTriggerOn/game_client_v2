#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXMath.h>

#include "debug_text.h"


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


void Collision_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Collision_DebugFinalize();
void Collision_DebugDraw(const Circle& circle,const DirectX::XMFLOAT4& color = {1.0f,1.0f,0.0f,1.0f});
void Collision_DebugDraw(const Box& box,const DirectX::XMFLOAT4& color = { 1.0f,1.0f,0.0f,1.0f });

#endif // COLLISION_H
