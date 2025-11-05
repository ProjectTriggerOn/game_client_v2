#include "player_cam_tps.h"

#include "direct3d.h"
#include "player.h"
#include "shader_3d.h"
#include "shader_field.h"
using namespace DirectX;
namespace 
{
	//XMMATRIX mtxView;

	XMFLOAT3 eyePosition{0.0f,0.0f,0.0f};
	XMFLOAT3 eyeDirection{};
	XMFLOAT3 upDirection{};

	constexpr float MOVE_SPEED = 1.0f;
	constexpr float CAMERA_ROT_SPEED = XMConvertToRadians(30.0f);

	XMFLOAT3 g_CameraFront{0.0f,0.0f,1.0f};
	XMFLOAT3 g_CameraRight{};
	XMFLOAT3 g_CameraUp{};

	XMFLOAT4X4 g_ViewMatrix{};
	XMFLOAT4X4 g_PerspectiveMatrix{};

	float g_fov;
}
void PlayerCamTps_Initialize()
{

}

void PlayerCamTps_Initialize(
	const DirectX::XMFLOAT3& position, 
	const DirectX::XMFLOAT3& front, 
	const DirectX::XMFLOAT3& right, 
	const DirectX::XMFLOAT3& up)
{
}

void PlayerCamTps_Finalize(void)
{
}

void PlayerCamTps_Update(double elapsed_time)
{
	XMVECTOR position = XMLoadFloat3(&Player_GetPosition()) - XMLoadFloat3(&Player_GetFront()) * 5.0f;
	position = XMVectorAdd(position, { 0.0f,3.0f,0.0f });
	XMVECTOR target = XMLoadFloat3(&Player_GetPosition());
	XMVECTOR front = XMVector3Normalize(target - position);

	XMStoreFloat3(&g_CameraFront, front);
	XMStoreFloat3(&eyePosition, position);

	XMMATRIX mtxView = XMMatrixLookAtLH(
		position, // 視点座標
		target, // 注視点座標
		{0.0f,1.0f,0.0f} // 上方向ベクトル
	);

	Shader_3D_SetViewMatrix(mtxView);
	Shader_Field_SetViewMatrix(mtxView);

	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float nearZ = 0.1f;
	float farZ = 1000.0f;

	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(
		1.0f,
		aspectRatio,
		nearZ,
		farZ
	);

	Shader_3D_SetProjectMatrix(mtxPerspective);
	Shader_Field_SetProjectMatrix(mtxPerspective);
}


const DirectX::XMFLOAT3& PlayerCamTps_GetFront()
{
	return g_CameraFront;
}

const DirectX::XMFLOAT3& PlayerCamTps_GetPosition()
{
	return eyePosition;
}


