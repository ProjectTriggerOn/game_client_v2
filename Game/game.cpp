#include "game.h"

#include "collision_world.h"
#include "cube.h"
#include "map.h"
#include "shader.h"
#include "camera.h"
#include "direct3d.h"
#include "infinite_grid.h"
#include "keyboard.h"
#include "key_logger.h"
#include "mesh_field.h"
#include "sampler.h"
#include "light.h"
#include "model.h"
#include "model_ani.h"
#include "ms_logger.h"
#include "player_cam_tps.h"
#include "player_cam_fps.h"
#include "player_fps.h"
#include "i_network.h"
#include "remote_player.h"
#include "input_producer.h"
#include "sky_dome.h"
#include "sprite.h"
#include "texture.h"
#include "fade.h"
#include "font.h"
#include "ui_widget.h"
#include "mouse.h"
#include <cwchar>
#include <vector>
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	int g_CrossHairTexId = -1;
	int g_CursorTexId = -1;
	int g_OverlayTexId = -1;  // white texture used for the settings dim overlay
#if defined(_DEBUG)
	bool isDebugCam = false;
	bool isDebugCollision = false;
#else
	constexpr bool isDebugCam = false;
	constexpr bool isDebugCollision = false;
#endif
	PlayerFps* g_PlayerFps;
	GameState g_GameState;
	
	// Correction state for debug display
	const char* g_CorrectionMode = "NONE";
	float g_CorrectionError = 0.0f;

	// Collision world
	CollisionWorld g_CollisionWorld;

	// ========================================================================
	// Lag compensation: view-tick tracking
	//
	// Client clock driving remote-player interpolation (advanced every frame
	// in Game_Update). File-scope so Game_GetViewTick() can map the current
	// interp-delayed render time back into server-tick space.
	// ========================================================================
	double g_ClientClock = 0.0;

	constexpr double SERVER_TICK_RATE = 32.0;  // matches PlayerFps/GameServer TICK_RATE (private there)

	// (receiveTime, serverTick) pairs for snapshots, same cadence as the
	// RemotePlayer snapshot buffers — lets us compute which (fractional)
	// server tick the interp-delayed render time corresponds to.
	struct TickTimeEntry
	{
		double receiveTime;
		uint32_t serverTick;
	};
	std::vector<TickTimeEntry> g_TickTimeBuffer;
	constexpr size_t TICK_TIME_BUFFER_MAX = 32;
}

// Global network debug info (populated from received snapshots)
NetworkDebugInfo g_NetDebugInfo;

void Game_Initialize()
{
	g_GameState = TITLE;

	g_CrossHairTexId = Texture_LoadFromFile(L"resource/texture/arr.png");
	g_CursorTexId    = Texture_LoadFromFile(L"resource/texture/cursor.png");
	g_OverlayTexId   = Texture_LoadFromFile(L"resource/texture/white.png");
	Font_Initialize();
	Widget_Initialize();
	Map_Initialize();
	Map_RegisterColliders(g_CollisionWorld);

	SkyDome_Initialize();
	g_PlayerFps = new PlayerFps();
	g_PlayerFps->Initialize({ -7.0f, 0.0f, -7.0f }, { 0.0f, 0.0f, 1.0f }, &g_CollisionWorld);

	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
	// Mouse mode is owned by MousePolicy_Apply (mouse_policy.h) — no direct calls here
	Fade_Initialize();
}

bool Game_WantsUICursor()
{
	// Free cursor while in the settings overlay or the debug TPS camera;
	// consumed by MousePolicy_Apply each frame.
	return g_GameState == SETTING || isDebugCam;
}

void Game_Update(double elapsed_time)
{
	// ========================================================================
	// ESC: toggle settings screen
	// ========================================================================
	// State flips only — cursor mode/visibility derive from state in
	// MousePolicy_Apply (mouse_policy.h)
	if (KeyLogger_IsTrigger(KK_ESCAPE))
	{
		g_GameState = (g_GameState != SETTING) ? SETTING : PLAY;
	}

	// In settings screen: skip all gameplay update
	if (g_GameState == SETTING)
	{
		return;
	}


#if defined(_DEBUG)
	if (KeyLogger_IsTrigger(KK_C)) {
		isDebugCam = !isDebugCam;
	}
	if (KeyLogger_IsTrigger(KK_F1)) {
		isDebugCollision = !isDebugCollision;
	}
#endif

	g_PlayerFps->Update(elapsed_time);
	
	// ========================================================================
	// Consume Snapshots from Network (works with both Mock and ENet)
	//
	// Local player uses client-side prediction - NO interpolation (causes lag).
	// Correction is handled via render offset inside PlayerFps.
	// ========================================================================
	extern INetwork* g_pNetwork;
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	extern InputProducer* g_pInputProducer;

	// Increment client clock EVERY FRAME for smooth interpolation
	g_ClientClock += elapsed_time;

	Snapshot snap;
	while (g_pNetwork && g_pNetwork->ReceiveSnapshot(snap))
	{
		// Apply server correction to local player
		g_PlayerFps->ApplyServerCorrection(snap.localPlayer);
		g_PlayerFps->SetTeam(snap.localPlayerTeam);

		// Feed server state to InputProducer (for jump-pending logic)
		if (g_pInputProducer)
		{
			g_pInputProducer->SetLastServerState(snap.localPlayer);
		}

		// Dispatch remote players from snapshot.
		// Clamp remotePlayerCount to the array bound — a malicious/corrupted
		// snapshot with count > MAX_PLAYERS-1 would otherwise OOB-read past
		// the Snapshot struct.
		const uint8_t remoteCount = (snap.remotePlayerCount < (MAX_PLAYERS - 1))
			? snap.remotePlayerCount
			: static_cast<uint8_t>(MAX_PLAYERS - 1);
		bool seenThisSnap[MAX_PLAYERS] = {};
		for (uint8_t i = 0; i < remoteCount; i++)
		{
			uint8_t rid = snap.remotePlayers[i].playerId;
			if (rid < MAX_PLAYERS)
			{
				seenThisSnap[rid] = true;
				g_RemotePlayerActive[rid] = true;
				g_RemotePlayers[rid].SetActive(true);
				g_RemotePlayers[rid].SetTeam(snap.remotePlayers[i].teamId);
				g_RemotePlayers[rid].PushSnapshot(snap.remotePlayers[i].state, g_ClientClock);
			}
		}
		// Deactivate players absent from this snapshot (disconnected)
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			if (g_RemotePlayerActive[i] && !seenThisSnap[i])
			{
				g_RemotePlayerActive[i] = false;
				g_RemotePlayers[i].SetActive(false);
			}
		}

		// Cache for debug display
		g_NetDebugInfo.tickDelta = snap.tickId - g_NetDebugInfo.prevServerTick;
		g_NetDebugInfo.prevServerTick = snap.tickId;
		g_NetDebugInfo.lastServerTick = snap.tickId;
		g_NetDebugInfo.lastServerTime = snap.serverTime;
		g_NetDebugInfo.lastServerState = snap.localPlayer;
		g_NetDebugInfo.hasData = true;
		g_NetDebugInfo.snapshotsThisSecond++;

		// Record (receiveTime, serverTick) for lag-comp view-tick computation.
		// A backward tick means the server restarted — stale mapping is invalid.
		if (!g_TickTimeBuffer.empty() && snap.tickId < g_TickTimeBuffer.back().serverTick)
		{
			g_TickTimeBuffer.clear();
		}
		g_TickTimeBuffer.push_back({ g_ClientClock, snap.tickId });
		if (g_TickTimeBuffer.size() > TICK_TIME_BUFFER_MAX)
		{
			g_TickTimeBuffer.erase(g_TickTimeBuffer.begin());
		}
	}

	// Update snapshot receive rate (once per second)
	g_NetDebugInfo.snapshotRateTimer += elapsed_time;
	if (g_NetDebugInfo.snapshotRateTimer >= 1.0)
	{
		g_NetDebugInfo.snapshotsPerSecond = g_NetDebugInfo.snapshotsThisSecond;
		g_NetDebugInfo.snapshotsThisSecond = 0;
		g_NetDebugInfo.snapshotRateTimer -= 1.0;
	}

	// Update debug info from player correction
	g_CorrectionMode = g_PlayerFps->GetCorrectionMode();
	g_CorrectionError = g_PlayerFps->GetCorrectionError();

	SkyDome_SetPosition(g_PlayerFps->GetPosition());

	if (isDebugCam)
	{
		PlayerCamTps_Update_Maya(elapsed_time, g_PlayerFps->GetPosition());
	}
	else {
		PlayerCamFps_Update(elapsed_time, g_PlayerFps->GetEyePosition());
	}

	// (KK_U manual MSLogger UI-mode toggle removed — MousePolicy_Apply now owns
	//  that flag and would override the toggle on the next frame anyway)

	// Update all active RemotePlayer instances (every frame for smooth interpolation)
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (g_RemotePlayerActive[i])
			g_RemotePlayers[i].Update(elapsed_time, g_ClientClock);
	}

	Fade_Update(elapsed_time);
}

void Game_Draw()
{
	Sampler_SetFilterAnisotropic();

	Light_SetAmbient({ 0.5f,0.5f,0.5f });

	XMFLOAT4X4 mtxView = isDebugCam ? PlayerCamTps_GetViewMatrix() : PlayerCamFps_GetViewMatrix();
	XMFLOAT4X4 mtxProj = isDebugCam ? PlayerCamTps_GetPerspectiveMatrix() : PlayerCamFps_GetProjectMatrix();
	XMMATRIX view = XMLoadFloat4x4(&mtxView);
	XMMATRIX proj = XMLoadFloat4x4(&mtxProj);
	XMFLOAT3 cam_pos = isDebugCam ? PlayerCamTps_GetPosition() : PlayerCamFps_GetPosition();

	Camera_SetMatrixToShader(view, proj);

	

	XMMATRIX mtxW = XMMatrixIdentity();

	mtxW = XMMatrixTranslation(0.0f,-1.0f,0.0f)* mtxW;

	XMVECTOR v{ 0.0f, -1.0f, 0.0f };
	v = XMVector3Normalize(v);
	XMFLOAT4 dir;
	XMStoreFloat4(&dir, v);
	Light_SetDirectionalWorld(dir, { 1.0f, 1.0f, 1.0f, 1.0f });

	PointLightList list{
	{
			{XMFLOAT3(0.0f,0.2f,0.0f),5.0f,XMFLOAT4(0.0f,1.0f,1.0f,1.0f)},
			{XMFLOAT3(2.0f,2.0f,0.0f),5.0f,XMFLOAT4(1.0f,0.0f,1.0f,1.0f)},
			{XMFLOAT3(-2.0f,0.2f,0.0f),5.0f,XMFLOAT4(1.0f,1.0f,0.0f,1.0f)},
			{XMFLOAT3(0.0f,0.2f,2.0f),5.0f,XMFLOAT4(0.0f,0.0f,1.0f,1.0f)},
	},
	4,
	XMFLOAT3(0,0,0)
	};

	PointLightList whiteList{
{
		{XMFLOAT3(0.0f,0.2f,0.0f),5.0f,XMFLOAT4(1.0f,1.0f,1.0f,1.0f)},
		{XMFLOAT3(2.0f,2.0f,0.0f),5.0f,XMFLOAT4(1.0f,1.0f,1.0f,1.0f)},
		{XMFLOAT3(-2.0f,0.2f,0.0f),5.0f,XMFLOAT4(1.0f,1.0f,1.0f,1.0f)},
		{XMFLOAT3(0.0f,0.2f,2.0f),5.0f,XMFLOAT4(1.0f,1.0f,1.0f,1.0f)},
	},
	4,
	XMFLOAT3(0,0,0)
	};

	Light_SetSpecularWorld(cam_pos, 4.0f, { 0.3f,0.3f,0.3f,1.0f });

	SkyDome_Draw();

	g_PlayerFps->Draw();

	// Draw all active Remote Players
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (g_RemotePlayerActive[i])
			g_RemotePlayers[i].Draw();
	}

	Cube_SetUVMode(CUBE_UV_PER_FACE);
	Map_Draw();

	// Debug draw: collision shapes (F3 toggle)
	if (isDebugCollision)
	{
		// Disable depth test so outlines are visible through map objects
		Direct3D_SetDepthEnable(false);

		Collision_DebugSetViewProj(view * proj);

		// Draw local player capsule (green)
		Capsule playerCapsule = g_PlayerFps->GetCapsule();
		Collision_DebugDraw(playerCapsule, { 0.0f, 1.0f, 0.0f, 1.0f });

		// Draw remote player capsules (red)
		for (int i = 0; i < MAX_PLAYERS; i++)
		{
			if (g_RemotePlayerActive[i] && !g_RemotePlayers[i].IsDead())
			{
				XMFLOAT3 rpos = g_RemotePlayers[i].GetRenderPosition();
				float height = g_RemotePlayers[i].GetHeight();
				float radius = 0.3f;
				Capsule remoteCapsule;
				remoteCapsule.pointA = { rpos.x, rpos.y + radius, rpos.z };
				remoteCapsule.pointB = { rpos.x, rpos.y + height - radius, rpos.z };
				remoteCapsule.radius = radius;
				Collision_DebugDraw(remoteCapsule, { 1.0f, 0.2f, 0.2f, 1.0f });
			}
		}

		// Draw all world AABB colliders
		for (const auto& collider : g_CollisionWorld.GetColliders())
		{
			XMFLOAT4 color = collider.isGround
				? XMFLOAT4{ 0.0f, 0.5f, 1.0f, 1.0f }   // blue for ground
				: XMFLOAT4{ 1.0f, 0.5f, 0.0f, 1.0f };   // orange for walls
			Collision_DebugDraw(collider.aabb, color);
		}

		// Draw shooting ray (yellow)
		XMFLOAT3 eyePos = g_PlayerFps->GetEyePosition();
		XMFLOAT3 fwd = { mtxView._13, mtxView._23, mtxView._33 };
		float rayLen = 200.0f;
		XMFLOAT3 rayEnd = {
			eyePos.x + fwd.x * rayLen,
			eyePos.y + fwd.y * rayLen,
			eyePos.z + fwd.z * rayLen
		};
		Collision_DebugDrawLine(eyePos, rayEnd, { 1.0f, 1.0f, 0.0f, 1.0f });

		// Restore depth test
		Direct3D_SetDepthEnable(true);

		// Restore 2D ortho projection after debug draw overwrote CB0
		Sprite_Begin();

		PlayerCamFps_Debug(*g_PlayerFps);
	}

	float sw = (float)Direct3D_GetBackBufferWidth();
	float sh = (float)Direct3D_GetBackBufferHeight();

	float x = (sw - 120.0f) / 2.0f;
	float y = (sh - 120.0f) / 2.0f;

	Sprite_Draw(g_CrossHairTexId, x, y, 120.0f, 120.0f);

	Direct3D_SetDepthEnable(false);

	if (MSLogger_IsUIMode()) {
		int mouse_x = MSLogger_GetXUI();
		int mouse_y = MSLogger_GetYUI();
		Sprite_Draw(g_CursorTexId, (float)mouse_x, (float)mouse_y, 32.0f, 32.0f);
	}

	// ========================================================================
	// HUD — bottom-right (hidden during settings screen)
	// ========================================================================
	if (g_GameState != SETTING)
	{
		constexpr float HUD_W  = 260.0f;
		constexpr float HUD_H  = 52.0f;
		constexpr float HUD_PAD = 20.0f;
		const XMFLOAT4  HUD_BG  = { 0.06f, 0.06f, 0.10f, 0.78f };

		float hudX = sw - HUD_W - HUD_PAD;
		float hudY = sh - HUD_H * 2.0f - HUD_PAD - 8.0f;

		// HP
		wchar_t hpBuf[32];
		swprintf_s(hpBuf, L"HP  %d / 200", (int)g_PlayerFps->GetHealth());
		Widget_DrawPanel(hudX, hudY, HUD_W, HUD_H, hpBuf, HUD_BG);

		// Ammo
		hudY += HUD_H + 8.0f;
		wchar_t ammoBuf[32];
		swprintf_s(ammoBuf, L"%d / %d", g_PlayerFps->GetAmmo(), g_PlayerFps->GetAmmoReserve());
		Widget_DrawPanel(hudX, hudY, HUD_W, HUD_H, ammoBuf, HUD_BG);
	}

	// ========================================================================
	// Settings overlay (drawn when in SETTING state)
	// ========================================================================
	if (g_GameState == SETTING)
	{
		float sw = static_cast<float>(Direct3D_GetBackBufferWidth());
		float sh = static_cast<float>(Direct3D_GetBackBufferHeight());

		// Full-screen dim
		//Sprite_Draw(g_OverlayTexId, 0.0f, 0.0f, sw, sh, { 0.04f, 0.04f, 0.06f, 0.72f });

		// Settings panel — centered
		constexpr float PW = 420.0f;
		constexpr float PH = 360.0f;
		float px = (sw - PW) * 0.5f;
		float py = (sh - PH) * 0.5f;

		// Panel background only (no centered title — would overlap buttons)
		Widget_DrawPanel(px, py, PW, PH, nullptr,
			{ 0.10f, 0.10f, 0.16f, 0.92f });

		// Title pinned to top of panel
		const wchar_t* title = L"SETTINGS";
		float titleX = px + (PW - Font_MeasureWidth(title)) * 0.5f;
		Font_Draw(title, titleX, py + 16.0f, { 1.0f, 1.0f, 1.0f, 1.0f });

		// Sensitivity slider — long and thick, upper quarter of screen
		constexpr float SW = 1000.0f;
		constexpr float SH = 72.0f;
		float sx = (sw - SW) * 0.5f;
		float sy = sh * 0.22f;
		float newSens = Widget_DrawSlider(sx, sy, SW, SH,
			PlayerCamFps_GetSensitivity(), 0.001f, 0.010f, L"Sensitivity");
		PlayerCamFps_SetSensitivity(newSens);

		// Resume button / Infinite Reserve toggle / Exit
		constexpr float BW = 260.0f;
		constexpr float BH = 52.0f;
		float bx = (sw - BW) * 0.5f;

		if (Widget_DrawButton(bx, py + 150.0f, BW, BH, L"Exit Game"))
		{
			PostQuitMessage(0);
		}
	}

	// Fade overlay (depth off so it covers everything including crosshair)
	Fade_Draw();

	Direct3D_SetDepthEnable(true);
}

void Game_Finalize()
{
	Camera_Finalize();
	PlayerCamTps_Finalize();
	PlayerCamFps_Finalize();
	g_PlayerFps->Finalize();
	ModelAni_Release(g_pModel0);
	Map_Finalize();
	Font_Finalize();
	Widget_Finalize();
	Fade_Finalize();
}

void Game_SetState(GameState state)
{
	g_GameState = state;
}

GameState Game_GetState()
{
	return g_GameState;
}

const char* Game_GetCorrectionMode()
{
	return g_CorrectionMode;
}

float Game_GetCorrectionError()
{
	return g_CorrectionError;
}

CollisionWorld* Game_GetCollisionWorld()
{
	return &g_CollisionWorld;
}

uint32_t Game_GetClientTick()
{
	return g_PlayerFps ? g_PlayerFps->GetClientTick() : 0;
}

void Game_GetViewTick(uint32_t& outTick, float& outFrac)
{
	// Map the interp-delayed render time (what the player actually SEES of
	// remote players) back into server-tick space, mirroring the bracketing
	// rules of RemotePlayer::Update so the reported tick matches the render.
	outTick = 0;  // sentinel: no data, server must not rewind
	outFrac = 0.0f;
	if (g_TickTimeBuffer.empty()) return;

	const double renderTime = g_ClientClock - RemotePlayer::INTERPOLATION_DELAY;

	// INTERP: find consecutive pair with t0 <= renderTime < t1.
	// Pairs with t0 >= t1 (snapshots drained in the same frame share one
	// receiveTime) are skipped — same rule as RemotePlayer::Update.
	for (size_t i = 0; i + 1 < g_TickTimeBuffer.size(); ++i)
	{
		const TickTimeEntry& a = g_TickTimeBuffer[i];
		const TickTimeEntry& b = g_TickTimeBuffer[i + 1];
		if (a.receiveTime >= b.receiveTime) continue;
		if (a.receiveTime <= renderTime && renderTime < b.receiveTime)
		{
			const double alpha = (renderTime - a.receiveTime) / (b.receiveTime - a.receiveTime);
			// Lerp in TICK space — b.serverTick - a.serverTick may be >1 on drops,
			// matching the position lerp RemotePlayer does across the same pair.
			const double fracTick = static_cast<double>(a.serverTick)
				+ static_cast<double>(b.serverTick - a.serverTick) * alpha;
			outTick = static_cast<uint32_t>(fracTick);
			outFrac = static_cast<float>(fracTick - static_cast<double>(outTick));
			return;
		}
	}

	// EXTRAP / SNAP: renderTime past the newest snapshot
	const TickTimeEntry& newest = g_TickTimeBuffer.back();
	if (renderTime >= newest.receiveTime)
	{
		double ahead = renderTime - newest.receiveTime;
		// Beyond the extrapolation limit RemotePlayer SNAPs to the newest
		// snapshot — report exactly that tick instead of extrapolating on.
		if (ahead > RemotePlayer::MAX_EXTRAPOLATION_TIME) ahead = 0.0;
		const double fracTick = static_cast<double>(newest.serverTick) + ahead * SERVER_TICK_RATE;
		outTick = static_cast<uint32_t>(fracTick);
		outFrac = static_cast<float>(fracTick - static_cast<double>(outTick));
	}
	// renderTime < oldest (WAIT mode) → outTick stays 0 (no compensation)
}

