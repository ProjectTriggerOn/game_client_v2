#pragma once
//=============================================================================
// billboard.h — camera-facing unit quad renderer (ported from LPSP particle_sys,
// camera decoupled: view matrix is injected via Billboard_SetCamera each frame).
//=============================================================================
#include <d3d11.h>
#include <DirectXMath.h>

void Billboard_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Billboard_Finalize(void);

// Inject the active camera's view matrix (FPS or TPS — decided by the caller).
// Stripping its translation and transposing gives the camera basis used to
// orient the quad toward the eye.
void Billboard_SetCamera(const DirectX::XMFLOAT4X4& view);

void Billboard_Draw(int texID, const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT2& scale, const DirectX::XMFLOAT2& pivot,
	const DirectX::XMFLOAT4& color);
