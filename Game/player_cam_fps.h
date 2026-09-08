#pragma once
#include <DirectXMath.h>

#include "mouse.h"
#include "player_fps.h"
#ifndef PLAYER_CAM_FPS_H
#define PLAYER_CAM_FPS_H

void PlayerCamFps_Initialize();
void PlayerCamFps_Finalize();
void PlayerCamFps_Update(double elapsed_time, const DirectX::XMFLOAT3& camera_position);

const DirectX::XMFLOAT3& PlayerCamFps_GetFront();
const DirectX::XMFLOAT3& PlayerCamFps_GetPosition();
void PlayerCamFps_SetPosition(const DirectX::XMFLOAT3& position);
void PlayerCamFps_SetFront(const DirectX::XMFLOAT3& front);

void PlayerCamFps_SetYaw(float yaw);
void PlayerCamFps_SetPitch(float pitch);

// Raw aim angles without recoil punch — for input reporting (the InputCmd
// must carry pure player intent; punch is visual-only and added later).
float PlayerCamFps_GetRawYaw();
float PlayerCamFps_GetRawPitch();

void PlayerCamFps_SetInvertY(bool invert = true);
bool PlayerCamFps_GetInvertY();

void PlayerCamFps_SetSensitivity(float s);
float PlayerCamFps_GetSensitivity();

// Recoil punch (COD model). The camera does NOT integrate its own copy: it
// stores the absolute offset it is handed, which PlayerFps pushes every frame
// from RecoilTotalOffsets(pool) — the same value the server ray-casts through
// (spec §2 WYSIWYG). Pitch is positive up, radians.
//
// It used to accumulate per-shot deltas and run its own exp(-decayHz*dt)
// recovery. That decayed the WHOLE accumulator while the pool decays only
// `punch` and never `shotKick`, so the rendered view sank below the server's
// aim line by shotKick — ~1.5° per magazine, growing all life (fixed 2026-09;
// covered by Game/tests/test_recoil_view_sync.cpp).
void PlayerCamFps_SetPunch(float punchPitch, float punchYaw);

// Read back the live rendered punch (pitch positive up, rad). The crosshair
// reads these so it tracks the rendered view 1:1 instead of the recoil pool.
void PlayerCamFps_GetPunch(float& punchPitch, float& punchYaw);


const DirectX::XMFLOAT4X4& PlayerCamFps_GetViewMatrix();
const DirectX::XMFLOAT4X4& PlayerCamFps_GetProjectMatrix();

void PlayerCamFps_Debug(const PlayerFps& pf);

#endif
