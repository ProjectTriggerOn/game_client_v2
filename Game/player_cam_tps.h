#pragma once
#ifndef PLAYER_CAM_TPS_H
#define PLAYER_CAM_TPS_H

#include <DirectXMath.h>

void PlayerCamTps_Initialize();
void PlayerCamTps_Finalize();
void PlayerCamTps_Update_Maya(double elapsed_time, const DirectX::XMFLOAT3& target);

const DirectX::XMFLOAT3& PlayerCamTps_GetPosition();
const DirectX::XMFLOAT4X4& PlayerCamTps_GetPerspectiveMatrix();
const DirectX::XMFLOAT4X4& PlayerCamTps_GetViewMatrix();

#endif
