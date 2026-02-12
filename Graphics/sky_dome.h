#pragma once
#include <DirectXMath.h>

bool SkyDome_Initialize();
void SkyDome_Finalize();
void SkyDome_SetPosition(const DirectX::XMFLOAT3& position);

void SkyDome_Draw();

