#include "game.h"


#include "cube.h"
#include "shader.h"
#include "camera.h"
#include "direct3d.h"
#include "font.h"
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
#include "result.h"
#include "sky_dome.h"
#include "sprite.h"
#include "stage_UI.h"
#include "target_point.h"
#include "texture.h"
#include "title_UI.h"
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	int g_CrossHairTexId = -1;
	int g_CursorTexId = -1;
	bool isDebugCam = false;
	Player_Fps* g_PlayerFps;
	StageUI* g_StageUI;
	TitleUI* g_TitleUI;
	ResultUI* g_ResultUI;
	PointSystem* g_pointSystem;
	GameState g_GameState;
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

	g_pModel0 = ModelAni_Load("resource/model/arms002.fbx");
	g_pModel = ModelLoad("resource/model/ball.fbx", 0.005f, false);
	g_CrossHairTexId = Texture_LoadFromFile(L"resource/texture/arr.png");
	g_CursorTexId = Texture_LoadFromFile(L"resource/texture/cursor.png");
	//ModelAni_SetAnimation(g_pModel0, 0);
	//g_pModel0 = ModelLoad("resource/model/(Legacy)arms_assault_rifle_01.fbx", 10.0f);
	//Player_Initialize({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,1.0f });
	Font_Initialize();
	SkyDome_Initialize();
	g_PlayerFps = new Player_Fps();
	g_PlayerFps->Initialize({ 0.0f,3.0f,-20.0f }, { 0.0f,0.0f,1.0f });

	g_StageUI = new StageUI();
	g_TitleUI = new TitleUI();
	g_ResultUI = new ResultUI();

	g_StageUI->Initialize();
	//g_StageUI->StartTimer();

	g_TitleUI->Initialize();
	g_ResultUI->Initialize();

	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
	//Ball_Initialize();
	g_pointSystem = new PointSystem(-5,5,2,7);
	g_pointSystem->GenerateInitialPoints(3);
	g_pointSystem->SetModel(g_pModel);

	Mouse_SetMode(MOUSE_POSITION_MODE_RELATIVE);
}

void Game_Update(double elapsed_time)
{
	//ModelAni_Update(g_pModel0, elapsed_time);
		// 1. Update Rotation from Mouse
	Mouse_SetVisible(false);

	if (g_GameState == READY && g_PlayerFps->GetFireCounter() == 2) {
		Game_SetState(COUNTDOWN);
	}

	if (g_GameState == TITLE ||
		g_GameState == PAUSE ||
		g_GameState == SETTING ||
		g_GameState == RESULT) {
		MSLogger_SetUIMode(true);
	}
	else {
		MSLogger_SetUIMode(false);
	}


	if (KeyLogger_IsTrigger(KK_C)) {
		isDebugCam = !isDebugCam;
	}

	g_PlayerFps->Update(elapsed_time);
	SkyDome_SetPosition(g_PlayerFps->GetPosition());

	if (isDebugCam)
	{
		PlayerCamTps_Update_Maya(elapsed_time);
	}
	else {
		PlayerCamFps_Update(elapsed_time, g_PlayerFps->GetEyePosition());

	}

	//Ball_UpdateAll(elapsed_time);

	if (MSLogger_IsTrigger(MBT_LEFT)) {

		Ray player_ray = { g_PlayerFps->GetEyePosition(), g_PlayerFps->GetFront() };

		CollisionResult result = g_pointSystem->CheckCollision(player_ray);

		if (result.isHit) {
			g_pointSystem->EliminateSpecificPoint(result.hitPoint);
		}
	}

	if (KeyLogger_IsTrigger(KK_U)) {
		MSLogger_SetUIMode(!MSLogger_IsUIMode());
	}

	if (g_GameState == TITLE) {

		g_TitleUI->Update(elapsed_time);
	}
	if (g_GameState == PLAY || g_GameState == COUNTDOWN) {
		g_StageUI->UpdateAccuracy(g_PlayerFps->GetFireCounter(), g_pointSystem->GetHitCount());
		g_StageUI->Update(elapsed_time);
	}

	if (g_GameState == RESULT){
		g_ResultUI->UpdateData(g_PlayerFps->GetFireCounter(), g_pointSystem->GetHitCount());
	g_ResultUI->Update(elapsed_time);
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

	MeshField_Draw(mtxW);


	g_pointSystem->DrawPoints();

	float sw = (float)Direct3D_GetBackBufferWidth();
	float sh = (float)Direct3D_GetBackBufferHeight();

	float x = (sw - 120.0f) / 2.0f;
	float y = (sh - 120.0f) / 2.0f;

	Sprite_Draw(g_CrossHairTexId, x, y, 120.0f, 120.0f);

	PlayerCamFps_Debug(*g_PlayerFps);

	Direct3D_SetDepthEnable(false);

	
	if (g_GameState == PLAY || g_GameState == COUNTDOWN) {
		g_StageUI->Draw();
	}


	if (g_GameState == TITLE) {
		g_TitleUI->Draw();
	}

	if (g_GameState == RESULT) {
		g_ResultUI->Draw();
	}

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
	if (state == PLAY) {
		g_StageUI->StartTimer();
	}
	if (state == COUNTDOWN) {
		g_StageUI->StartCountdownTimer();
	}
}

GameState Game_GetState()
{
	return g_GameState;
}





