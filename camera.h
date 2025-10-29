#pragma once
#ifndef CAMERA_H
#define CAMERA_H
#include <DirectXMath.h>


void Camera_Initialize();
void Camera_Initialize(
	const DirectX::XMFLOAT3& position,
	const DirectX::XMFLOAT3& front,
	const DirectX::XMFLOAT3& right,
	const DirectX::XMFLOAT3& up
	);
void Camera_Finalize(void);
void Camera_Update(double elapsed_time);

const DirectX::XMFLOAT4X4& Camera_GetViewMatrix();
const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix();

const DirectX::XMFLOAT3& Camera_GetFront();
const DirectX::XMFLOAT3& Camera_GetPosition();

void Camera_SetFov();

void Camera_Debug();



#endif