#include "camera.h"
#include <DirectXMath.h>
#include "direct3d.h"
#include "shader_3d.h"
using namespace DirectX;

void Camera_Initialize()
{
}

void Camera_Finalize()
{
}

void Camera_Update()
{
	//ビュー座標変換行列を設定
	XMMATRIX mtxView = XMMatrixLookAtLH(
		{ 2.0f,2.0f,-5.0f },// 視点座標
		{ 0.0f,0.0f,0.0f }, // 注視点座標
		{ 0.0f,1.0f,0.0f } // 上方向ベクトル
	);

	Shader_3D_SetViewMatrix(mtxView);

	float fovAngleY = XMConvertToRadians(60.0f);
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float nearZ = 0.1f;
	float farZ = 100.0f;

	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(
		fovAngleY,
		aspectRatio,
		nearZ,
		farZ
	);

	Shader_3D_SetProjectMatrix(mtxPerspective);
}
