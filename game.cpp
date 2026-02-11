#include "game.h"


#include "cube.h"
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
#include "mock_server.h"
#include "remote_player.h"
#include "sky_dome.h"
#include "sprite.h"
#include "texture.h"
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	int g_CrossHairTexId = -1;
	int g_CursorTexId = -1;
	bool isDebugCam = false;
	Player_Fps* g_PlayerFps;
	GameState g_GameState;
	
	// Correction state for debug display
	const char* g_CorrectionMode = "NONE";
	float g_CorrectionError = 0.0f;
}

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
	SkyDome_Initialize();
	g_PlayerFps = new Player_Fps();
	g_PlayerFps->Initialize({ 0.0f,0.0f,-20.0f }, { 0.0f,0.0f,1.0f });

	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
	//Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
}

void Game_Update(double elapsed_time)
{

	Mouse_SetVisible(false);


	if (KeyLogger_IsTrigger(KK_C)) {
		isDebugCam = !isDebugCam;
	}

	g_PlayerFps->Update(elapsed_time);
	
	// ========================================================================
	// Server Reconciliation (Prediction + Correction)
	// 
	// Local player uses client-side prediction - NO interpolation (causes lag).
	// Correction is handled via render offset inside Player_Fps.
	// ========================================================================
	extern MockServer* g_pMockServer;
	if (g_pMockServer)
	{
		const NetPlayerState& serverState = g_pMockServer->GetPlayerState();
		g_PlayerFps->ApplyServerCorrection(serverState);
		
		// Update debug info from player
		g_CorrectionMode = g_PlayerFps->GetCorrectionMode();
		g_CorrectionError = g_PlayerFps->GetCorrectionError();
	}

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

	// Update RemotePlayer (Interpolation + Extrapolation with snapshot buffer)
	extern RemotePlayer* g_pRemotePlayer;
	static uint32_t lastRemotePlayerTick = 0;  // Track last pushed tick
	static double clientClock = 0.0;  // Continuous client-side clock for interpolation
	if (g_pRemotePlayer && g_pMockServer)
	{
		// Increment client clock EVERY FRAME (this is the key fix!)
		// Server time only updates at 32Hz, but we need smooth per-frame interpolation
		clientClock += elapsed_time;
		
		// For demo: RemotePlayer mirrors local player's server state offset by 5 units
		NetPlayerState remoteState = g_pMockServer->GetPlayerState();

		// Offset position so we can see both players
		//remoteState.position.x = -remoteState.position.x;  // Offset so we can see both
		//remoteState.position.z = -remoteState.position.z;  // Offset so we can see both
		//remoteState.yaw = fmodf(remoteState.yaw + XM_PI, XM_2PI); // Face opposite direction

		remoteState.position.x = remoteState.position.x + 5.0f;  // Offset so we can see both

		// Only push snapshot when server tick changes (avoid flooding buffer)
		// Use serverTime for snapshot timestamp (when the data was valid)
		if (remoteState.tickId != lastRemotePlayerTick)
		{
			lastRemotePlayerTick = remoteState.tickId;
			// Snapshot timestamp = when this data was generated (server tick time)
			double snapshotTime = g_pMockServer->GetServerTime();
			g_pRemotePlayer->PushSnapshot(remoteState, snapshotTime);
		}
		
		// Update with interpolation using CONTINUOUS client clock (every frame)
		// renderTime = clientClock - interpolationDelay
		g_pRemotePlayer->Update(elapsed_time, clientClock);
	}

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

	// Draw Remote Player
	extern RemotePlayer* g_pRemotePlayer;
	if (g_pRemotePlayer)
	{
		g_pRemotePlayer->Draw();
	}

	MeshField_Draw(mtxW);

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

