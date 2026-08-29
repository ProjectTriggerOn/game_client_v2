#pragma once
//=============================================================================
// decal.h — ring-buffer bullet hole decals (fixed pool, oldest overwritten).
// Draws with the billboard shader but uses its own surface-aligned world
// matrix (decals stick to walls; they do not face the eye).
//=============================================================================
#include <DirectXMath.h>

constexpr int DECAL_MAX = 128;

void Decal_Initialize();
void Decal_Finalize();

// Register a bullet hole at `hitPos`, oriented by the surface `normal`
// (expected unit length), offset slightly along the normal to avoid z-fighting.
void Decal_Create(const DirectX::XMFLOAT3& hitPos, const DirectX::XMFLOAT3& normal);

void Decal_Draw(); // depth-write-off state expected (see ImpactFx_Draw)

int Decal_GetCount(); // number of currently held decals (for the debug panel)
