#include "camera.h"
#include <DirectXMath.h>
#include <iomanip>


#include <sstream>

#include "debug_text.h"
#include "direct3d.h"
#include "key_logger.h"
#include "mouse.h"
#include "shader_3d.h"
using namespace DirectX;

namespace 
{
	XMMATRIX mtxView;

	XMFLOAT3 eyePosition{};
	XMFLOAT3 eyeDirection{};
	XMFLOAT3 upDirection{};

	constexpr float MOVE_SPEED = 1.0f;
	constexpr float MOUSE_ROT_SPEED = 0.008f;
	constexpr float WHEEL_ROT_SPEED = 0.004f;
	constexpr float CAMERA_ROT_SPEED = XMConvertToRadians(30.0f);

	XMFLOAT3 g_Camerafront{};
	XMFLOAT3 g_Cameraright{};
	XMFLOAT3 g_CameraUp{};

	

	XMFLOAT4X4 g_ViewMatrix{};
	XMFLOAT4X4 g_PerspectiveMatrix{};

	hal::DebugText* g_DebugText = nullptr;

}

void Camera_Initialize()
{

	XMStoreFloat4x4(&g_ViewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&g_PerspectiveMatrix, XMMatrixIdentity());

#if defined(_DEBUG) || defined(DEBUG)

	g_DebugText = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 900.0f,
		0, 0,
		0.0f, 16.0f
	);

#endif // _DEBUG || DEBUG
	eyePosition = { 0.0f,0.0f,-5.0f };
	eyeDirection = { 0.0f,0.0f,1.0f };
	upDirection = { 0.0f,1.0f,0.0f };

	g_Camerafront = { 0.0f,0.0f,1.0f };
	g_Cameraright = { 1.0f,0.0f,0.0f };
	g_CameraUp = { 0.0f,1.0f,0.0f };

}

void Camera_Initialize(DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& front, DirectX::XMFLOAT3& up, DirectX::XMFLOAT3& right)
{

	Camera_Initialize();
	eyePosition = position;
	g_Camerafront = front;
	g_CameraUp = up;
	g_Cameraright = right;
	XMStoreFloat4x4(&g_ViewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&g_PerspectiveMatrix, XMMatrixIdentity());
}

void Camera_Finalize()
{
	delete g_DebugText;
}

void Camera_Update(double elapsed_time)
{
	XMVECTOR front = XMLoadFloat3(&g_Camerafront);
	XMVECTOR right = XMLoadFloat3(&g_Cameraright);
	XMVECTOR up    = XMLoadFloat3(&g_CameraUp);
	XMVECTOR position = XMLoadFloat3(&eyePosition);

	if (KeyLogger_IsPressed(KK_DOWN))
	{
		XMMATRIX rot = XMMatrixRotationAxis(right, CAMERA_ROT_SPEED * elapsed_time);
		front = XMVector3TransformNormal(front, rot);
		front = XMVector3Normalize(front);
		up = XMVector3Cross(front, right);
	}
	if (KeyLogger_IsPressed(KK_UP))
	{
		XMMATRIX rot = XMMatrixRotationAxis(right, -CAMERA_ROT_SPEED * elapsed_time);
		front = XMVector3TransformNormal(front, rot);
		front = XMVector3Normalize(front);
		up = XMVector3Cross(front, right);
	}

	if (KeyLogger_IsPressed(KK_LEFT))
	{
		XMMATRIX rot = XMMatrixRotationY(-CAMERA_ROT_SPEED * elapsed_time);
		up = XMVector3Normalize(XMVector3TransformNormal(up,rot));
		front = XMVector3TransformNormal(front, rot);
		front = XMVector3Normalize(front);
		right = XMVector3Normalize(XMVector3Cross(up,front)* 
			XMVECTOR{1.0,0.0,1.0});

	}
	if (KeyLogger_IsPressed(KK_RIGHT))
	{
		XMMATRIX rot = XMMatrixRotationY(CAMERA_ROT_SPEED * elapsed_time);
		up = XMVector3Normalize(XMVector3TransformNormal(up, rot));
		front = XMVector3TransformNormal(front, rot);
		front = XMVector3Normalize(front);
		right = XMVector3Normalize(XMVector3Cross(up, front)*
			XMVECTOR { 1.0, 0.0, 1.0 });
	}
	if (KeyLogger_IsPressed(KK_W))
	{
		position += front * MOVE_SPEED * elapsed_time;


		
	}
	if (KeyLogger_IsPressed(KK_A))
	{
		position += -right * MOVE_SPEED * elapsed_time;

	}
	if (KeyLogger_IsPressed(KK_S))
	{
		position += -front * MOVE_SPEED * elapsed_time;
	}
	if (KeyLogger_IsPressed(KK_D))
	{
		position += right * MOVE_SPEED * elapsed_time;
	}
	if (KeyLogger_IsPressed(KK_SPACE))
	{
		position += up * MOVE_SPEED * elapsed_time;
	}
	if (KeyLogger_IsPressed(KK_LEFTCONTROL))
	{
		position -= up * MOVE_SPEED * elapsed_time;
	}

	XMStoreFloat3(&eyePosition, position);
	XMStoreFloat3(&g_Camerafront, front);
	XMStoreFloat3(&g_CameraUp, up);
	//right = XMVector3Cross(front, up);
	XMStoreFloat3(&g_Cameraright, right);

	mtxView = XMMatrixLookAtLH(
		position,// 視点座標
		position + front, // 注視点座標
		up // 上方向ベクトル
	);

	XMStoreFloat4x4(&g_ViewMatrix, mtxView);

	Shader_3D_SetViewMatrix(mtxView);

	float fovAngleY = XMConvertToRadians(60.0f);
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float nearZ = 0.1f;
	float farZ = 1000.0f;

	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(
		fovAngleY,
		aspectRatio,
		nearZ,
		farZ
	);
	XMStoreFloat4x4(&g_PerspectiveMatrix, mtxPerspective);

	Shader_3D_SetProjectMatrix(mtxPerspective);
}

const DirectX::XMFLOAT4X4& Camera_GetViewMatrix()
{
	return g_ViewMatrix;
}

const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix()
{
	return g_PerspectiveMatrix;
}

const DirectX::XMFLOAT3& Camera_GetFront()
{
	return g_Camerafront;
}

const DirectX::XMFLOAT3& Camera_GetPosition()
{
	return eyePosition;
}

void Camera_Debug()
{
#if defined(_DEBUG) || defined(DEBUG)

	std::stringstream ss;
	ss << "Camera Position:x = " << std::fixed << std::setprecision(3) << eyePosition.x;
	ss << " y = " << std::fixed << std::setprecision(3) << eyePosition.y;
	ss << " z = " << std::fixed << std::setprecision(3) << eyePosition.z << std::endl;

	ss << "Camera Front:x = " << std::fixed << std::setprecision(3) << g_Camerafront.x;
	ss << " y = " << std::fixed << std::setprecision(3) << g_Camerafront.y;
	ss << " z = " << std::fixed << std::setprecision(3) << g_Camerafront.z << std::endl;

	ss << "Camera Up:x = " << std::fixed << std::setprecision(3) << g_CameraUp.x;
	ss << " y = " << std::fixed << std::setprecision(3) << g_CameraUp.y;
	ss << " z = " << std::fixed << std::setprecision(3) << g_CameraUp.z << std::endl;

	ss << "Camera Right:x = " << std::fixed << std::setprecision(3) << g_Cameraright.x;
	ss << " y = " << std::fixed << std::setprecision(3) << g_Cameraright.y;
	ss << " z = " << std::fixed << std::setprecision(3) << g_Cameraright.z << std::endl;

	g_DebugText->SetText(ss.str().c_str());
	g_DebugText->Draw();
	g_DebugText->Clear();

#endif

}
