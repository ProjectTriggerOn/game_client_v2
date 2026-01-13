#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_3DUnlit_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_3DUnlit_Finalize();

void Shader_3DUnlit_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3DUnlit_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3DUnlit_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3DUnlit_SetProjectMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3DUnlit_SetColor(const DirectX::XMFLOAT4& color);

void Shader_3DUnlit_Begin();


