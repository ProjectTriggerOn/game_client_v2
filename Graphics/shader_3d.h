
#ifndef SHADER_3D_H
#define	SHADER_3D_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_3D_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_3D_Finalize();

void Shader_3D_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_SetProjectMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_SetColor(const DirectX::XMFLOAT4& color);

void Shader_3D_Begin();

#endif 
