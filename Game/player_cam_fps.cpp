#include "player_cam_fps.h"
#include "key_logger.h"
#include "direct3d.h"
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
#include "i_network.h"
#include "input_producer.h"
#include "remote_player.h"
#include "game.h"
#include "config.h"
#include "audio.h"

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
	float g_Sensitivity = 0.002f;

	// Recoil punch (COD model): VISUAL camera offset, pushed here every frame
	// from the recoil pool (PlayerCamFps_SetPunch). Never written into
	// g_cameraYaw/g_cameraPitch — the player's aim stays pure; punch is added
	// when building the front vector and the view matrix.
	float g_punchPitch = 0.0f;
	float g_punchYaw   = 0.0f;

	// Vertical FOV in radians (driven live by config key display.fov, in degrees)
	float g_Fov = XM_PIDIV4;   // 45° default
}

void PlayerCamFps_Initialize()
{
	g_cameraYaw = 0.0f;
	g_cameraPitch = 0.0f;
	g_CameraFront = { 0.0f, 0.0f, 1.0f };

	// Config subscriptions (docs §9.2: each module owns its keys).
	// Initialize() runs on every scene (re)entry, but subscription must happen
	// exactly once — Subscribe also fires immediately if the key has a value,
	// so persisted settings are applied right here.
	static bool s_subscribed = false;
	if (!s_subscribed) {
		s_subscribed = true;
		auto& cfg = Config::GetInstance();
		cfg.Subscribe("input.sensitivity",
		              [](const ConfigValue& v) { g_Sensitivity = (float)v.AsFloat(); });
		cfg.Subscribe("input.invert_y",
		              [](const ConfigValue& v) { g_invertY = v.AsBool(); });
		// FOV adjustment is DISABLED for now: changing FOV distorts the first-person
		// viewmodel (weapon/arms), which needs a separate viewmodel FOV compensation
		// that isn't implemented yet. g_Fov stays at its XM_PIDIV4 default so the
		// projection keeps the original 45°. Re-enable by uncommenting this once the
		// viewmodel compensation lands (and un-hide the FOV row in settings.html).
		// cfg.Subscribe("display.fov",
		//               [](const ConfigValue& v) { g_Fov = XMConvertToRadians((float)v.AsFloat()); });
	}

	// Built in every configuration: its one consumer, PlayerCamFps_Debug, runs
	// under game.cpp's isDebugCollision, and that is now a runtime toggle in
	// Release as well as Debug (F1). Nothing is drawn until the key is pressed.
	// The "if (!g_DebugText) return;" in PlayerCamFps_Debug still guards the
	// case where construction failed — don't remove it.
	g_DebugText = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		L"resource/texture/consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
		0.0f, 20.0f,
		0, 0,
		0.0f, 16.0f
	);
}

void PlayerCamFps_Finalize()
{
	delete g_DebugText;
	g_DebugText = nullptr;
}

void PlayerCamFps_Update([[maybe_unused]] double elapsed_time, const DirectX::XMFLOAT3& position)
{
	int dx = MSLogger_GetX();
	int dy = MSLogger_GetY();

	// Apply rotation
	g_cameraYaw += dx * g_Sensitivity;
	if (g_invertY)
	{
		g_cameraPitch -= dy * g_Sensitivity;
	}
	else
	{
		g_cameraPitch += dy * g_Sensitivity;
	}

	// Clamp pitch to avoid flipping
	constexpr float PITCH_LIMIT = XM_PIDIV2 - 0.01f;
	g_cameraPitch = std::max(-PITCH_LIMIT, std::min(g_cameraPitch, PITCH_LIMIT));

	// 2. Calculate Camera Front Vector — RENDERED angles include the recoil
	// punch (visual only; the raw aim angles above stay untouched for input,
	// UI, and the InputCmd).  WYSIWYG: the rendered view IS the aim line the
	// server ray-casts through.
	const float renderedYaw   = g_cameraYaw + g_punchYaw;
	const float renderedPitch = g_cameraPitch + g_punchPitch;
	float x = cosf(renderedPitch) * sinf(renderedYaw);
	float y = sinf(renderedPitch);
	float z = cosf(renderedPitch) * cosf(renderedYaw);

	XMVECTOR front = XMVector3Normalize(XMVectorSet(x, y, z, 0.0f));
	XMStoreFloat3(&g_CameraFront, front);

	// 3. Update Camera Position
	// Direct assignment: The input position is now treated as the Eye/Camera position.
	// The offset logic should be handled by the caller (e.g. PlayerFps class).
	g_CameraPosition = position;
	DirectX::XMVECTOR vPos = XMLoadFloat3(&g_CameraPosition);

	// 4. Create View Matrix
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX view = XMMatrixLookToLH(vPos, front, up);

	XMStoreFloat4x4(&g_ViewMatrix, view);

	// 6. Projection Matrix
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float fov = g_Fov; // vertical FOV (radians), from config key display.fov
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

void PlayerCamFps_SetYaw(float yaw)
{
	g_cameraYaw = yaw;
}

void PlayerCamFps_SetPitch(float pitch)
{
	constexpr float PITCH_LIMIT = DirectX::XM_PIDIV2 - 0.01f;
	if (pitch > PITCH_LIMIT)  pitch = PITCH_LIMIT;
	if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
	g_cameraPitch = pitch;
}

float PlayerCamFps_GetRawYaw()
{
	return g_cameraYaw;
}

float PlayerCamFps_GetRawPitch()
{
	return g_cameraPitch;
}

void PlayerCamFps_SetPunch(float punchPitch, float punchYaw)
{
	// Pure store — the recoil pool is the single truth source and owns both
	// the accumulate and the decay (see player_cam_fps.h).
	g_punchPitch = punchPitch;
	g_punchYaw   = punchYaw;
}

void PlayerCamFps_GetPunch(float& punchPitch, float& punchYaw)
{
	punchPitch = g_punchPitch;
	punchYaw   = g_punchYaw;
}

void PlayerCamFps_SetInvertY(bool invert)
{
	g_invertY = invert;
}

bool PlayerCamFps_GetInvertY()
{
	return g_invertY;
}

void PlayerCamFps_SetSensitivity(float s)
{
	g_Sensitivity = s;
}

float PlayerCamFps_GetSensitivity()
{
	return g_Sensitivity;
}

const DirectX::XMFLOAT4X4& PlayerCamFps_GetViewMatrix()
{
	return g_ViewMatrix;
}

const DirectX::XMFLOAT4X4& PlayerCamFps_GetProjectMatrix()
{
	return g_ProjectionMatrix;
}

void PlayerCamFps_Debug(const PlayerFps& pf)
{
	if (!g_DebugText) return;

	// Access global NetworkDebugInfo (populated from received snapshots)
	extern NetworkDebugInfo g_NetDebugInfo;
	extern INetwork* g_pNetwork;

	std::stringstream ss;

	// ---- Network Quality ----
	// Fields are grouped several to a line throughout this overlay. The screen
	// holds 33 lines at this font size, and the local player's own readout used
	// to spend 31 of them before reaching the remote list — so in a full lobby
	// the remotes, which are the thing this overlay exists to diagnose, sat
	// entirely below the bottom edge. A line fits about 120 characters and these
	// fields are short, so the room was there all along, sideways.
	ss << "=== Network ===\n";
	if (g_pNetwork)
	{
		// ENet packetLoss is fixed-point (value / 65536 = fraction)
		float lossPercent = g_pNetwork->GetPacketLoss() * 100.0f / 65536.0f;
		ss << "Connected: " << (g_pNetwork->IsConnected() ? "YES" : "NO")
		   << "   RTT: " << g_pNetwork->GetRTT() << "ms"
		   << "   PacketLoss: " << std::fixed << std::setprecision(1) << lossPercent << "%\n";
		ss << "InputsSent: " << g_pNetwork->GetTotalInputsSent()
		   << "   SnapQueue: " << g_pNetwork->GetSnapshotQueueSize() << "\n";
	}
	ss << "SnapRate: " << g_NetDebugInfo.snapshotsPerSecond << "/s (expect 32)"
	   << "   TickDelta: " << g_NetDebugInfo.tickDelta << " (expect 1)\n";

	// ---- Lag compensation (what we report in InputCmd.viewTick) ----
	{
		uint32_t viewTick = 0;
		float viewFrac = 0.0f;
		Game_GetViewTick(viewTick, viewFrac);
		if (viewTick != 0)
		{
			// How far behind "now" the viewed world is = how far the server
			// will rewind hitboxes for our shots (client tick ≈ server tick)
			const double lagTicks = static_cast<double>(Game_GetClientTick())
				- (static_cast<double>(viewTick) + static_cast<double>(viewFrac));
			ss << "LagComp: viewing tick " << viewTick
			   << " (-" << std::fixed << std::setprecision(1) << lagTicks
			   << "t / " << std::setprecision(0) << (lagTicks * 1000.0 / 32.0)
			   << "ms behind)\n";
		}
		else
		{
			ss << "LagComp: no view data\n";
		}
	}

	// ---- Server Info ----
	ss << "\n=== Server (32Hz) ===\n";
	if (g_NetDebugInfo.hasData)
	{
		const NetPlayerState& srvState = g_NetDebugInfo.lastServerState;
		ss << "Tick: " << g_NetDebugInfo.lastServerTick
		   << "   Time: " << std::fixed << std::setprecision(1) << g_NetDebugInfo.lastServerTime << "s"
		   << "   Pos: " << std::setprecision(1)
		   << srvState.position.x << ", "
		   << srvState.position.y << ", "
		   << srvState.position.z << "\n";
	}
	else
	{
		ss << "NO DATA\n";
	}

	// ---- Correction ----
	ss << "\n=== Correction ===  Mode: " << Game_GetCorrectionMode()
	   << "   Error: " << std::fixed << std::setprecision(3) << Game_GetCorrectionError() << "m\n";

	// ---- Input ----
	extern InputProducer* g_pInputProducer;
	ss << "=== Input (C->S) ===";
	if (g_pInputProducer)
	{
		const InputCmd& cmd = g_pInputProducer->GetLastInputCmd();
		ss << "  MoveAxis: " << std::fixed << std::setprecision(1)
		   << cmd.moveAxisX << ", " << cmd.moveAxisY << "   Buttons: ";
		if (cmd.buttons & InputButtons::FIRE) ss << "FIRE ";
		if (cmd.buttons & InputButtons::ADS) ss << "ADS ";
		if (cmd.buttons & InputButtons::JUMP) ss << "JUMP ";
		if (cmd.buttons & InputButtons::SPRINT) ss << "SPRINT ";
		if (cmd.buttons == InputButtons::NONE) ss << "NONE";
	}
	ss << "\n";

	// ---- Player ----
	ss << "\n=== Player ===\n";
	ss << "Team: " << (pf.GetTeam() == PlayerTeam::RED ? "RED" : "BLUE")
	   << "   Health: " << (int)pf.GetHealth() << "/200" << (pf.IsDead() ? " [DEAD]" : "")
	   << "   " << pf.GetPlayerState() << " / " << pf.GetWeaponState()
	   << "   FireCounter: " << pf.GetFireCounter() << " (Srv: " << g_NetDebugInfo.lastServerState.fireCounter << ")\n";

	// Audio readout: voice pressure is the first thing to go wrong once a
	// firefight gets busy, and the last-derivation event count shows the
	// snapshot derivation working — or storming — at a glance. Sits with the
	// local player because that is what it describes: the listener IS the local
	// player, and the remote-player blocks below are a variable-length list that
	// would otherwise push these two lines off the bottom in a full lobby.
	ss << "AUDIO " << (Audio_IsAvailable() ? "on" : "SILENT")
	   << " voices=" << Audio_ActiveVoiceCount()
	   << " events=" << Game_LastAudioEventCount() << "\n";
	// Listener pose actually feeding the spatialiser (see main.cpp's
	// Audio_SetListener call): turn in place and these numbers should track
	// the player's own position/facing frame to frame -- if they don't, the
	// listener has come unhooked from the camera again.
	ss << "AUDIO listener pos=(" << std::fixed << std::setprecision(2)
	   << g_CameraPosition.x << "," << g_CameraPosition.y << "," << g_CameraPosition.z
	   << ") front=(" << g_CameraFront.x << "," << g_CameraFront.y << "," << g_CameraFront.z << ")\n";

	// ---- Remote Players ----
	// One line each, not a six-line block each. The overlay has room for about
	// 33 lines at this font size; a full lobby is nine remotes, and at seven
	// lines apiece they needed 63 — so every one of them fell off the bottom of
	// the screen and the block showed nothing at all. Nothing is dropped here,
	// only abbreviated: the fields are short numbers and there is far more
	// horizontal room than vertical.
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	int activeRemotes = 0;
	for (int rpi = 0; rpi < MAX_PLAYERS; rpi++)
		if (g_RemotePlayerActive[rpi] && g_RemotePlayers[rpi].IsActive()) activeRemotes++;

	ss << "\n=== Remotes (" << activeRemotes << ") ===\n";
	for (int rpi = 0; rpi < MAX_PLAYERS; rpi++)
	{
		if (!g_RemotePlayerActive[rpi] || !g_RemotePlayers[rpi].IsActive()) continue;
		RemotePlayer& rp = g_RemotePlayers[rpi];
		// SyncMode is padded so the columns after it line up — the modes are
		// 4 to 6 characters (WAIT / SNAP / INIT / INTERP / EXTRAP / NODATA).
		ss << "[" << rpi << "] "
		   << (rp.GetTeam() == PlayerTeam::RED ? "RED " : "BLU ")
		   << std::left << std::setw(6) << rp.GetSyncMode() << std::right
		   << " buf=" << rp.GetBufferSize()
		   << " t=" << std::fixed << std::setprecision(2) << rp.GetLerpFactor()
		   << " d=" << std::setprecision(0) << (rp.GetInterpolationDelay() * 1000.0) << "ms";
		if (rp.IsStuck()) ss << " STUCK";
		ss << "\n";
	}

	g_DebugText->SetText(ss.str().c_str());
	g_DebugText->Draw();
	g_DebugText->Clear();
}

