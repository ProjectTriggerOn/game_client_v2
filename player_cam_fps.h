#pragma once
#include <DirectXMath.h>
#ifndef PLAYER_CAM_FPS_H
#define PLAYER_CAM_FPS_H

void PlayerCamFps_Initialize();
void PlayerCamFps_Finalize();
void PlayerCamFps_Update(double elapsed_time);
void PlayerCamFps_Update(double elapsed_time, const DirectX::XMFLOAT3& camera_position);

const DirectX::XMFLOAT3& PlayerCamFps_GetFront();
const DirectX::XMFLOAT3& PlayerCamFps_GetPosition();
void PlayerCamFps_SetPosition(const DirectX::XMFLOAT3& position);
void PlayerCamFps_SetFront(const DirectX::XMFLOAT3& front);

void PlayerCamFps_SetInvertY(bool invert = true);
bool PlayerCamFps_GetInvertY();


const DirectX::XMFLOAT4X4& PlayerCamFps_GetViewMatrix();
const DirectX::XMFLOAT4X4& PlayerCamFps_GetProjectMatrix();

#endif
