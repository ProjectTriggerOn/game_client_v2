#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_InfiniteGrid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_InfiniteGrid_Finalize();

void Shader_InfiniteGrid_SetProjectMatrix(const DirectX::XMMATRIX& matrix);
void Shader_InfiniteGrid_SetWorldMatrix(const DirectX::XMMATRIX& matrix);

void Shader_InfiniteGrid_Begin();

