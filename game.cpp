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
#include "player.h"
#include "player_cam_tps.h"
#include "player_cam_fps.h"
#include "player_fps.h"
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
	MODEL* g_pModel = nullptr;
	MODEL_ANI* g_pModel0 = nullptr;
	bool isDebugCam = false;
	Player_Fps* g_PlayerFps;
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
	g_pModel0 = ModelAni_Load("resource/model/arms002.fbx");
	//ModelAni_SetAnimation(g_pModel0, 0);
	//g_pModel0 = ModelLoad("resource/model/(Legacy)arms_assault_rifle_01.fbx", 10.0f);
	//Player_Initialize({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,1.0f });
	g_PlayerFps = new Player_Fps();
	g_PlayerFps->Initialize({ 0.0f,3.0f,0.0f }, { 0.0f,0.0f,1.0f });
	Camera_Initialize();
	PlayerCamTps_Initialize();
	PlayerCamFps_Initialize();
	PlayerCamFps_SetInvertY(true);
}

void Game_Update(double elapsed_time)
{
	//ModelAni_Update(g_pModel0, elapsed_time);
		// 1. Update Rotation from Mouse
	Mouse_State ms;
	Mouse_GetState(&ms);


	if (KeyLogger_IsTrigger(KK_C)) {
		isDebugCam = !isDebugCam;
	}

	g_PlayerFps->Update(elapsed_time,ms);

	if (isDebugCam)
	{
		PlayerCamTps_Update_Maya(elapsed_time);
	}
	else {
		PlayerCamFps_Update(elapsed_time,g_PlayerFps->GetEyePosition(),ms);
		
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

	g_PlayerFps->Draw();

	//Light_SetPointLightByList(whiteList);

	Light_SetSpecularWorld(cam_pos, 4.0f, { 0.3f,0.3f,0.3f,1.0f });

	//ModelDraw(g_pModel, mtxW);

	MeshField_Draw(mtxW);

	//InfiniteGrid_Draw();

	//Cube_Draw(mtxW);

	PlayerCamFps_Debug(*g_PlayerFps);

}

void Game_Finalize()
{
	Camera_Finalize();
	Light_Finalize();
	InfiniteGrid_Finalize();
	//Player_Finalize();
	PlayerCamTps_Finalize();
	PlayerCamFps_Finalize();
	g_PlayerFps->Finalize();
	ModelAni_Release(g_pModel0);
}




