#pragma once
//=============================================================================
// shader_billboard.h — billboard rendering shader (b0 world / b3 UV in VS;
// PS b0 color tint). view/proj (b1/b2) are bound globally by
// Camera_SetMatrixToShader every frame — the setters here are no-ops kept for
// interface parity with the other shader modules.
//=============================================================================
#include <d3d11.h>
#include <DirectXMath.h>

struct UVParameter
{
	DirectX::XMFLOAT2 scale;
	DirectX::XMFLOAT2 translation;
};

bool Shader_Billboard_Initialize();
void Shader_Billboard_Finalize();

void Shader_Billboard_SetWorldMatrix(const DirectX::XMMATRIX& matrix);
void Shader_Billboard_SetViewMatrix([[maybe_unused]] const DirectX::XMMATRIX& matrix);
void Shader_Billboard_SetProjectMatrix([[maybe_unused]] const DirectX::XMMATRIX& matrix);
void Shader_Billboard_SetColor(const DirectX::XMFLOAT4& color);
void Shader_Billboard_SetUVParameter(const UVParameter& parameter);
void Shader_Billboard_Begin();
