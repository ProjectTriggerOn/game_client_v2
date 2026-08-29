#pragma once
#include <DirectXMath.h>

// Loads the sky dome model. If assetPath is null or empty, falls back to
// "resource/model/sky.fbx" — the historical default used before per-map
// environments landed. Returns false on model load failure.
bool SkyDome_Initialize(const char* assetPath = nullptr);
void SkyDome_Finalize();
void SkyDome_SetPosition(const DirectX::XMFLOAT3& position);

void SkyDome_Draw();
