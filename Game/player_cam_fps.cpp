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
#include <iomanip>
#include <ostream>
#include <sstream>

#include "ms_logger.h"
#include "net_common.h"
#include "input_producer.h"
#include "remote_player.h"
#include "game.h"

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
	hal::DebugText* g_DebugText = nullptr;

	// Mouse sensitivity
	constexpr float SENSITIVITY = 0.002f;
}

void PlayerCamFps_Initialize()
{
	g_cameraYaw = 0.0f;
	g_cameraPitch = 0.0f;
	g_CameraFront = { 0.0f, 0.0f, 1.0f };
#if defined(_DEBUG) || defined(DEBUG)

	g_DebugText = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 20.0f,
		0, 0,
		0.0f, 16.0f
	);

#endif // _DEBUG || DEBUG
}

void PlayerCamFps_Finalize()
{
}

void PlayerCamFps_Update(double elapsed_time)
{
	// 1. Update Rotation from Mouse
	Mouse_State ms;
	Mouse_GetState(&ms);

	Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);

	// Calculate delta mouse movement
	int dx = 0;
	int dy = 0;

	if (ms.positionMode == MOUSE_POSITION_MODE_RELATIVE)
	{
		dx = MSLogger_GetX();
		dy = MSLogger_GetY();
	}
	else
	{
		static int lastMouseX = MSLogger_GetX();
		static int lastMouseY = MSLogger_GetY();
		static bool firstMouse = true;

		if (firstMouse)
		{
			lastMouseX = MSLogger_GetX();
			lastMouseY = MSLogger_GetY();
			firstMouse = false;
		}

		dx = MSLogger_GetX() - lastMouseX;
		dy = MSLogger_GetY() - lastMouseY;
		lastMouseX = MSLogger_GetX();
		lastMouseY = MSLogger_GetY();
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

	XMStoreFloat4x4(&g_ViewMatrix, view);

	// 6. Projection Matrix
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float fov = XM_PIDIV4; // 45 degrees
	float nearZ = 0.1f;
	float farZ = 1000.0f;
	XMMATRIX projection = XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

	XMStoreFloat4x4(&g_ProjectionMatrix, projection);
}

void PlayerCamFps_Update(double elapsed_time, const DirectX::XMFLOAT3& position)
{

	//Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);

	// Calculate delta mouse movement
	int dx = 0;
	int dy = 0;

	if (MSLogger_GetPositionMode() == MOUSE_POSITION_MODE_RELATIVE)
	{
		dx = MSLogger_GetX();
		dy = MSLogger_GetY();
	}
	else
	{
		static int lastMouseX = MSLogger_GetX();
		static int lastMouseY = MSLogger_GetY();
		static bool firstMouse = true;

		if (firstMouse)
		{
			lastMouseX = MSLogger_GetX();
			lastMouseY = MSLogger_GetY();
			firstMouse = false;
		}

		dx = MSLogger_GetX() - lastMouseX;
		dy = MSLogger_GetY() - lastMouseY;
		lastMouseX = MSLogger_GetX();
		lastMouseY = MSLogger_GetY();
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

	// 3. Update Camera Position
	// Direct assignment: The input position is now treated as the Eye/Camera position.
	// The offset logic should be handled by the caller (e.g. Player_Fps class).
	g_CameraPosition = position;
	DirectX::XMVECTOR vPos = XMLoadFloat3(&g_CameraPosition);

	// 4. Create View Matrix
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookToLH(vPos, front, up);

	XMStoreFloat4x4(&g_ViewMatrix, view);

	// 6. Projection Matrix
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float fov = XM_PIDIV4; // 45 degrees
	float nearZ = 0.1f;
	float farZ = 1000.0f;
	XMMATRIX projection = XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

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

void PlayerCamFps_SetPosition(const DirectX::XMFLOAT3& position)
{
	g_CameraPosition = position;
}

void PlayerCamFps_SetFront(const DirectX::XMFLOAT3& front)
{
	g_CameraFront = front;
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

void PlayerCamFps_Debug(const Player_Fps& pf)
{
#if defined(_DEBUG) || defined(DEBUG)

	// Access global NetworkDebugInfo (populated from received snapshots)
	extern NetworkDebugInfo g_NetDebugInfo;

	std::stringstream ss;

	// Server Info Section
	ss << "=== Server (32Hz) ===\n";
	if (g_NetDebugInfo.hasData)
	{
		ss << "ServerTick: " << g_NetDebugInfo.lastServerTick << "\n";
		ss << "ServerTime: " << std::fixed << std::setprecision(1)
		   << g_NetDebugInfo.lastServerTime << "s\n";

		// Server authoritative position
		const NetPlayerState& srvState = g_NetDebugInfo.lastServerState;
		ss << "ServerPos: " << std::fixed << std::setprecision(1)
		   << srvState.position.x << ", "
		   << srvState.position.y << ", "
		   << srvState.position.z << "\n";
	}
	else
	{
		ss << "Server: NO DATA\n";
	}
	
	// Correction Info
	ss << "\n=== Correction ===\n";
	ss << "Mode: " << Game_GetCorrectionMode() << "\n";
	ss << "Error: " << std::fixed << std::setprecision(3) << Game_GetCorrectionError() << "m\n";
	
	ss << "\n=== Input (C->S) ===\n";
	extern InputProducer* g_pInputProducer;
	if (g_pInputProducer)
	{
		const InputCmd& cmd = g_pInputProducer->GetLastInputCmd();
		ss << "MoveAxis: " << std::fixed << std::setprecision(1) 
		   << cmd.moveAxisX << ", " << cmd.moveAxisY << "\n";
		ss << "Buttons: ";
		if (cmd.buttons & InputButtons::FIRE) ss << "FIRE ";
		if (cmd.buttons & InputButtons::ADS) ss << "ADS ";
		if (cmd.buttons & InputButtons::JUMP) ss << "JUMP ";
		if (cmd.buttons & InputButtons::SPRINT) ss << "SPRINT ";
		if (cmd.buttons == InputButtons::NONE) ss << "NONE";
		ss << "\n";
	}
	
	ss << "\n=== Player ===\n";
	ss << "PlayerState: " << pf.GetPlayerState() << "\n";
	ss << "WeaponState: " << pf.GetWeaponState() << "\n";
	ss << "CurrentAnimation: " << pf.GetCurrentAniName() << "\n";
	ss << "FireCounter: " << pf.GetFireCounter() << "\n";
	ss << "AniDuration: " << std::fixed << std::setprecision(2) << pf.GetCurrentAniDuration() << "s\n";
	ss << "AniProgress: " << std::fixed << std::setprecision(2) << pf.GetCurrentAniProgress() * 100.0f << "%\n";
	
	// RemotePlayer Debug (show all active remote players)
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	for (int rpi = 0; rpi < MAX_PLAYERS; rpi++)
	{
		if (!g_RemotePlayerActive[rpi] || !g_RemotePlayers[rpi].IsActive()) continue;
		RemotePlayer& rp = g_RemotePlayers[rpi];
		ss << "\n=== RemotePlayer[" << rpi << "] ===\n";
		ss << "SyncMode: " << rp.GetSyncMode();
		if (rp.IsStuck()) ss << " [STUCK!]";
		ss << "\n";
		ss << "Buffer: " << rp.GetBufferSize() << " snapshots\n";
		ss << "LerpT: " << std::fixed << std::setprecision(3) << rp.GetLerpFactor() << "\n";
		ss << "InterpDelay: " << std::fixed << std::setprecision(1) << (rp.GetInterpolationDelay() * 1000.0) << "ms\n";
		ss << "RenderTime: " << std::fixed << std::setprecision(2) << rp.GetLastRenderTime() << "s\n";
		ss << "SnapTimes: " << std::fixed << std::setprecision(2)
		   << rp.GetOldestSnapshotTime() << " - "
		   << rp.GetNewestSnapshotTime() << "\n";
	}

	g_DebugText->SetText(ss.str().c_str());
	g_DebugText->Draw();
	g_DebugText->Clear();

#endif
}

