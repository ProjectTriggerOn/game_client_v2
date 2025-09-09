#ifndef SHADER_3D_H
#define	SHADER_3D_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_Finalize();

void Shader_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_SetProjectMatrix(const DirectX::XMMATRIX& matrix);

void Shader_Begin();

#endif 
