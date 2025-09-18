#ifndef CIRCLE_H
#define CIRCLE_H

#include <DirectXMath.h>

#include <d3d11.h>

struct CircleD
{
	DirectX::XMFLOAT3 center; // 圆心坐标
	float radius; // 半径
};
void Circle_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Circle_DebugFinalize();
void Circle_DebugDraw(const CircleD& circle, const DirectX::XMFLOAT4& color = { 1.0f,1.0f,0.0f,1.0f });

#endif // COLLISION_H