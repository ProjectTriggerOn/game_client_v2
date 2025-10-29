#pragma once
#ifndef LIGHT_H
#define LIGHT_H
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
//点光源
struct PointLight
{
	DirectX::XMFLOAT3 LightPosition;
	float range;
	DirectX::XMFLOAT4 Color;
	//float SpecularStrength;
	//XMFLOAT3 SpecularColor;
};

//点光源リスト
struct PointLightList
{
	PointLight pointLights[4];
	int numPointLights;
	DirectX::XMFLOAT3 padding;
};

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Light_Finalize(void);
void Light_SetAmbient(const DirectX::XMFLOAT3& color);
void Light_SetDirectionalWorld(
	const DirectX::XMFLOAT4& direction, 
	const DirectX::XMFLOAT4& color
	);

void Light_SetSpecularWorld(
	const DirectX::XMFLOAT3& camera_position,
	const float& specular_power,
	const DirectX::XMFLOAT4& specular_color
);

void Light_SetPointLightByList(
	const PointLightList& pList
);

void Light_SetPointLightCount(int count);

void Light_SetPointLightWorldByCount(
	int index,
	const XMFLOAT3& position,
	float range,
	const XMFLOAT3& color);

#endif
