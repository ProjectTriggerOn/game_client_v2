#pragma once
#ifndef CUBE_H
#define CUBE_H

#include <d3d11.h>
#include <DirectXMath.h>
#include "collision.h"

// UV mapping modes
enum CubeUVMode
{
	CUBE_UV_ATLAS,    // Current: each face maps to a different atlas region
	CUBE_UV_PER_FACE, // Each face uses full [0,0]-[1,1] texture
};

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Cube_Finalize(void);
void Cube_Draw(int texID, const DirectX::XMMATRIX& mtxW);
void Cube_Update(double elapsed_time);
void Cube_SetUVMode(CubeUVMode mode);
CubeUVMode Cube_GetUVMode();

AABB Cube_GetAABB(const DirectX::XMFLOAT3& cubePos);


#endif