#pragma once
//=============================================================================
// impact_fx.h — gameplay glue: local fire counter polling -> raycast against
// the collision world -> bullet hole decal + spark burst. Also owns the draw
// ordering (depth test on / write off) for decals and particles.
//=============================================================================
#include <DirectXMath.h>

void ImpactFx_Initialize();
void ImpactFx_Finalize();

// Call from Game_Update (every frame; internally no-ops unless playing).
void ImpactFx_Update();

// Inject the active camera's view (decided by isDebugCam in Game_Draw) and
// draw decals + particles. Pair with Direct3D_SetDepthWriteEnable(false).
void ImpactFx_SetCamera(const DirectX::XMFLOAT4X4& view);
void ImpactFx_Draw();

void ImpactFx_DebugInfo(int& decalCount, int& particleCount);
