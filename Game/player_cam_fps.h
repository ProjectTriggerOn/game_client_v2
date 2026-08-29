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

void PlayerCamFps_SetInvertY(bool invert = true);
bool PlayerCamFps_GetInvertY();

void PlayerCamFps_SetSensitivity(float s);
float PlayerCamFps_GetSensitivity();

// Recoil punch (COD model). AddPunch accumulates a per-shot visual kick
// (pitch is positive up); DecayPunch advances exponential recovery at
// frame rate (dt = frame delta). Both fire-rate safe.
void PlayerCamFps_AddPunch(float dPitch, float dYaw);
void PlayerCamFps_DecayPunch(float dt);


const DirectX::XMFLOAT4X4& PlayerCamFps_GetViewMatrix();
const DirectX::XMFLOAT4X4& PlayerCamFps_GetProjectMatrix();

void PlayerCamFps_Debug(const PlayerFps& pf);

#endif
