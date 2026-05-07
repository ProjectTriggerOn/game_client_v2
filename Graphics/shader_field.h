#pragma once

#ifndef SHADER_Field_H
#define	SHADER_Field_H

#include <d3d11.h>
#include <DirectXMath.h>

bool Shader_Field_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Shader_Field_Finalize();

void Shader_Field_SetMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Field_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Field_SetViewMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Field_SetProjectMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Field_SetColor(const DirectX::XMFLOAT4& color);

void Shader_Field_Begin();

#endif 
