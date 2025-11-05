#pragma once
#include <DirectXMath.h>
#ifndef PLAYER_CAM_TPS_H
#define PLAYER_CAM_TPS_H

void PlayerCamTps_Initialize();
void PlayerCamTps_Initialize(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& front,
	const DirectX::XMFLOAT3& right,
	const DirectX::XMFLOAT3& up
);
void PlayerCamTps_Finalize();
void PlayerCamTps_Update(double elapsed_time);

const DirectX::XMFLOAT3& PlayerCamTps_GetFront();
const DirectX::XMFLOAT3& PlayerCamTps_GetPosition();





#endif

