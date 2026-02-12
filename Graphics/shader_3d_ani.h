#ifndef SHADER_3D_ANI_H
#define SHADER_3D_ANI_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_3D_Ani_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_3D_Ani_Finalize();

void Shader_3D_Ani_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_Ani_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_Ani_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_Ani_SetProjectMatrix(const DirectX::XMMATRIX& matrix);
void Shader_3D_Ani_SetColor(const DirectX::XMFLOAT4& color);
void Shader_3D_Ani_SetBoneMatrices(const DirectX::XMFLOAT4X4* matrices, int count);

void Shader_3D_Ani_Begin();

#endif
