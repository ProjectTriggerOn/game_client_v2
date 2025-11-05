#include "game.h"
#include "cube.h"
#include "shader.h"
#include "grid.h"
#include "camera.h"
#include "circle.h"
#include "key_logger.h"
#include "mesh_field.h"
#include "sampler.h"
#include "light.h"
#include "model.h"
#include "player.h"
#include "player_cam_tps.h"
using namespace DirectX;

namespace{

	double g_AccumulatedTime = 0.0;
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
	

	Player_Initialize({ 0.0f,0.0f,0.0f }, { 0.0f,0.0f,1.0f });
	PlayerCamTps_Initialize();
}

void Game_Update(double elapsed_time)
{
	//Camera_Update(elapsed_time);
	Player_Update(elapsed_time);
	PlayerCamTps_Update(elapsed_time);

}

void Game_Draw()
{
	Sampler_SetFilterAnisotropic();

	Light_SetAmbient({ 0.3f,0.3f,0.3f });

	XMVECTOR v{ 0.0f,-1.0f,0.0f };
	v = XMVector3Normalize(v);
	XMFLOAT4 dir;
	XMStoreFloat4(&dir, v);
	Light_SetDirectionalWorld(dir, { 1.0f,0.9f,0.7f,1.0f });
	//Light_SetDirectionalWorld({ 0.0f,-1.0f,0.0f,0.0f }, { 1.0f,0.9f,0.7f,1.0f });//方向光
	//Light_SetDirectionalWorld({ 0.0f,-1.0f,0.0f,0.0f }, { 0.3f,0.25f,0.2f,1.0f });//方向光

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

	Light_SetPointLightByList(list);

	XMMATRIX mtxWorld = XMMatrixIdentity();

	Player_Draw();

	Light_SetSpecularWorld(Camera_GetPosition(), 10.0f,{ 0.3f,0.3f,0.3f,1.0f });

	MeshField_Draw(mtxWorld);

	//Camera_Debug();
}

void Game_Finalize()
{
	Camera_Finalize();
	Light_Finalize();
}




