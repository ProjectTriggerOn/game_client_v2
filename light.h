#pragma once
#ifndef LIGHT_H
#define LIGHT_H
#include <d3d11.h>
#include <DirectXMath.h>


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

#endif
