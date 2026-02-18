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
#include "player.h"
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
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	int g_CrossHairTexId = -1;
	int g_CursorTexId = -1;
	bool isDebugCam = false;
	bool isDebugCollision = false;
	Player_Fps* g_PlayerFps;
	GameState g_GameState;
	
	// Correction state for debug display
	const char* g_CorrectionMode = "NONE";
	float g_CorrectionError = 0.0f;

	// Collision world
	CollisionWorld g_CollisionWorld;
}

// Global network debug info (populated from received snapshots)
NetworkDebugInfo g_NetDebugInfo;

void Game_Initialize()
{
	
	//Camera_Initialize(
	//	{1.0f,2,-2},
	//	{-0.291f,-0.777f,0.558f},
	//	{ 0.887f,0,0.462f },
	//	{-0.359f,0.629f,0.689f}
	//);
	//Camera_Initialize();
	
	//g_pModel = ModelLoad("resource/model/test.fbx", 0.1f,false);
	g_GameState = TITLE;

	g_CrossHairTexId = Texture_LoadFromFile(L"resource/texture/arr.png");
	g_CursorTexId = Texture_LoadFromFile(L"resource/texture/cursor.png");
	//ModelAni_SetAnimation(g_pModel0, 0);
	//g_pModel0 = ModelLoad("resource/model/(Legacy)arms_assault_rifle_01.fbx", 10.0f);
	//Player_Initialize({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,1.0f });
	// Initialize map and register colliders
	Map_Initialize();
	Map_RegisterColliders(g_CollisionWorld);

	SkyDome_Initialize();
	g_PlayerFps = new Player_Fps();
	g_PlayerFps->Initialize({ -7.0f, 0.0f, -7.0f }, { 0.0f, 0.0f, 1.0f }, &g_CollisionWorld);

	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
	Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
	Fade_Initialize();
}

void Game_Update(double elapsed_time)
{

	Mouse_SetVisible(false);


	if (KeyLogger_IsTrigger(KK_C)) {
		isDebugCam = !isDebugCam;
	}
	if (KeyLogger_IsTrigger(KK_F3)) {
		isDebugCollision = !isDebugCollision;
	}

	g_PlayerFps->Update(elapsed_time);
	
	// ========================================================================
	// Consume Snapshots from Network (works with both Mock and ENet)
	//
	// Local player uses client-side prediction - NO interpolation (causes lag).
	// Correction is handled via render offset inside Player_Fps.
	// ========================================================================
	extern INetwork* g_pNetwork;
	extern RemotePlayer g_RemotePlayers[];
	extern bool g_RemotePlayerActive[];
	extern InputProducer* g_pInputProducer;
	static double clientClock = 0.0;

	// Increment client clock EVERY FRAME for smooth interpolation
	clientClock += elapsed_time;

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

		// Dispatch remote players from snapshot
		for (uint8_t i = 0; i < snap.remotePlayerCount; i++)
		{
			uint8_t rid = snap.remotePlayers[i].playerId;
			if (rid < MAX_PLAYERS)
			{
				g_RemotePlayerActive[rid] = true;
				g_RemotePlayers[rid].SetActive(true);
				g_RemotePlayers[rid].SetTeam(snap.remotePlayers[i].teamId);
				g_RemotePlayers[rid].PushSnapshot(snap.remotePlayers[i].state, clientClock);
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
		PlayerCamTps_Update_Maya(elapsed_time);
	}
	else {
		PlayerCamFps_Update(elapsed_time, g_PlayerFps->GetEyePosition());

	}

	if (KeyLogger_IsTrigger(KK_U)) {
		MSLogger_SetUIMode(!MSLogger_IsUIMode());
	}

	// Update all active RemotePlayer instances (every frame for smooth interpolation)
	for (int i = 0; i < MAX_PLAYERS; i++)
	{
		if (g_RemotePlayerActive[i])
			g_RemotePlayers[i].Update(elapsed_time, clientClock);
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

	//ModelAni_SetAnimation(g_pModel0, 1);
	//ModelAni_Draw(g_pModel0, mtxW,true);
	

	XMVECTOR v{ 0.0f,-1.0f,0.0f };
	v = XMVector3Normalize(v);
	XMFLOAT4 dir;
	XMStoreFloat4(&dir, v);
	Light_SetDirectionalWorld(dir, { 1.0f,1.0f,1.0f,1.0f });
	////Light_SetDirectionalWorld({ 0.0f,-1.0f,0.0f,0.0f }, { 1.0f,0.9f,0.7f,1.0f });//方向光
	////Light_SetDirectionalWorld({ 0.0f,-1.0f,0.0f,0.0f }, { 0.3f,0.25f,0.2f,1.0f });//方向光

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
			if (g_RemotePlayerActive[i])
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
	}

	float sw = (float)Direct3D_GetBackBufferWidth();
	float sh = (float)Direct3D_GetBackBufferHeight();

	float x = (sw - 120.0f) / 2.0f;
	float y = (sh - 120.0f) / 2.0f;

	Sprite_Draw(g_CrossHairTexId, x, y, 120.0f, 120.0f);

	PlayerCamFps_Debug(*g_PlayerFps);

	Direct3D_SetDepthEnable(false);

	if (MSLogger_IsUIMode()) {
		int mouse_x = MSLogger_GetXUI();
		int mouse_y = MSLogger_GetYUI();
		Sprite_Draw(g_CursorTexId, (float)mouse_x, (float)mouse_y, 32.0f, 32.0f);
	}

	// Fade overlay (depth off so it covers everything including crosshair)
	Fade_Draw();

	Direct3D_SetDepthEnable(true);
}

void Game_Finalize()
{
	Camera_Finalize();
	//Player_Finalize();
	PlayerCamTps_Finalize();
	PlayerCamFps_Finalize();
	g_PlayerFps->Finalize();
	ModelAni_Release(g_pModel0);
	Map_Finalize();
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

