#include "player_cam_fps.h"
#include "key_logger.h"
#include "direct3d.h"
#include "player.h"
#include "shader_3d.h"
#include "shader_field.h"
#include "shader_infinite.h"
#include "mouse.h"
#include "shader_3d_ani.h"
#include <algorithm>

using namespace DirectX;

namespace
{
	XMFLOAT3 g_CameraPosition{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };
	
	float g_cameraYaw = 0.0f;
	float g_cameraPitch = 0.0f;
	bool g_invertY = false;

	XMFLOAT4X4 g_ViewMatrix{};
	XMFLOAT4X4 g_ProjectionMatrix{};

	// Mouse sensitivity
	constexpr float SENSITIVITY = 0.002f;
}

void PlayerCamFps_Initialize()
{
	g_cameraYaw = 0.0f;
	g_cameraPitch = 0.0f;
	g_CameraFront = { 0.0f, 0.0f, 1.0f };
	Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
}

void PlayerCamFps_Finalize()
{
}

void PlayerCamFps_Update(double elapsed_time)
{
	// 1. Update Rotation from Mouse
	Mouse_State ms;
	Mouse_GetState(&ms);

	// Calculate delta mouse movement
	int dx = 0;
	int dy = 0;

	if (ms.positionMode == MOUSE_POSITION_MODE_RELATIVE)
	{
		dx = ms.x;
		dy = ms.y;
	}
	else
	{
		static int lastMouseX = ms.x;
		static int lastMouseY = ms.y;
		static bool firstMouse = true;

		if (firstMouse)
		{
			lastMouseX = ms.x;
			lastMouseY = ms.y;
			firstMouse = false;
		}

		dx = ms.x - lastMouseX;
		dy = ms.y - lastMouseY;
		lastMouseX = ms.x;
		lastMouseY = ms.y;
	}

	// Apply rotation
	g_cameraYaw += dx * SENSITIVITY;
	if (g_invertY)
	{
		g_cameraPitch -= dy * SENSITIVITY;
	}
	else
	{
		g_cameraPitch += dy * SENSITIVITY;
	}

	// Clamp pitch to avoid flipping
	constexpr float PITCH_LIMIT = XM_PIDIV2 - 0.01f;
	g_cameraPitch = std::max(-PITCH_LIMIT, std::min(g_cameraPitch, PITCH_LIMIT));

	// 2. Calculate Camera Front Vector
	// Spherical coordinates to Cartesian coordinates
	float x = cosf(g_cameraPitch) * sinf(g_cameraYaw);
	float y = sinf(g_cameraPitch);
	float z = cosf(g_cameraPitch) * cosf(g_cameraYaw);

	XMVECTOR front = XMVector3Normalize(XMVectorSet(x, y, z, 0.0f));
	XMStoreFloat3(&g_CameraFront, front);

	// 3. Update Camera Position (Lock to Player Head)
	XMFLOAT3 playerPos = Player_GetPosition();
	// Assuming player height is around 2.0 units, eye level at 1.7
	// Adjust this offset based on your player model scale
	XMVECTOR eyePos = XMLoadFloat3(&playerPos) + XMVectorSet(0.0f, 1.7f, 0.0f, 0.0f); 
	XMStoreFloat3(&g_CameraPosition, eyePos);

	// 4. Create View Matrix
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookToLH(eyePos, front, up);

	// 5. Set Matrices to Shaders
	Shader_InfiniteGrid_SetViewMatrix(view);

	XMStoreFloat4x4(&g_ViewMatrix, view);

	// 6. Projection Matrix
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float fov = XM_PIDIV4; // 45 degrees
	float nearZ = 0.1f;
	float farZ = 1000.0f;
	XMMATRIX projection = XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);


	Shader_InfiniteGrid_SetProjectMatrix(projection);

	XMStoreFloat4x4(&g_ProjectionMatrix, projection);
}

const DirectX::XMFLOAT3& PlayerCamFps_GetFront()
{
	return g_CameraFront;
}

const DirectX::XMFLOAT3& PlayerCamFps_GetPosition()
{
	return g_CameraPosition;
}

void PlayerCamFps_SetInvertY(bool invert)
{
	g_invertY = invert;
}

bool PlayerCamFps_GetInvertY()
{
	return g_invertY;
}

const DirectX::XMFLOAT4X4& PlayerCamFps_GetViewMatrix()
{
	return g_ViewMatrix;
}

const DirectX::XMFLOAT4X4& PlayerCamFps_GetProjectMatrix()
{
	return g_ProjectionMatrix;
}
