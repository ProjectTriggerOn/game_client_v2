#pragma once
#ifndef CUBE_H
#define CUBE_H

#include <d3d11.h>
#include <DirectXMath.h>
#include "collision.h"

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Cube_Finalize(void);
void Cube_Draw(DirectX::XMMATRIX mtxW);
void Cube_Update(double elapsed_time);

AABB Cube_GetAABB(const DirectX::XMFLOAT3& cubePos);


#endif