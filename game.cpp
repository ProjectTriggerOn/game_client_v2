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
using namespace DirectX;

namespace{
	XMFLOAT3 g_CubePosition{};
	XMFLOAT3 g_CubeVelocity{};
	MODEL* g_pModel = nullptr;
	MODEL* g_pModel0 = nullptr;
	float g_angle = 0.0f;
}

void Game_Initialize()
{
	
	Camera_Initialize(
		{1.0f,2,-2},
		{-0.291f,-0.777f,0.558f},
		{ 0.887f,0,0.462f },
		{-0.359f,0.629f,0.689f}
	);
	//Camera_Initialize();
	g_pModel = ModelLoad("resource/model/test.fbx",0.1f);
	g_pModel0 = ModelLoad("resource/model/heli.fbx",0.5f);
}

void Game_Update(double elapsed_time)
{
	g_angle += static_cast<float>(elapsed_time) * XMConvertToRadians(90.0f);
	
	Cube_Update(elapsed_time);
	Camera_Update(elapsed_time);
	if (KeyLogger_IsTrigger(KK_SPACE))
	{
		
	}


	// Move the cube forward in the direction the camera is facing
	XMVECTOR cube_position = XMLoadFloat3(&g_CubePosition);
	cube_position = XMLoadFloat3(&Camera_GetFront()) * 10.0f * static_cast<float>(elapsed_time) + cube_position;
	XMStoreFloat3(&g_CubePosition, cube_position);
}

void Game_Draw()
{

	Light_SetAmbient({ 0.0f,0.0f,0.0f });
	XMVECTOR v{ 0.0f,-1.0f,0.0f };
	v = XMVector3Normalize(v);
	XMFLOAT4 dir;
	XMStoreFloat4(&dir, v);
	Light_SetDirectionalWorld(dir,{ 0.3f,0.25f,0.2f,1.0f });
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

	//Light_SetPointLightByList(list);

	Light_SetPointLightCount(3);

	//点光源回転
	XMMATRIX plRotation = XMMatrixRotationY(g_angle);

	XMFLOAT3 plPos0,plPos1,plPos2;

	XMStoreFloat3(&plPos0, XMVector3Transform({ 3.0f,2.0f,-3.0f }, plRotation));

	XMStoreFloat3(&plPos1, XMVector3Transform({ -5.0f,3.0f,-5.0f }, plRotation));

	XMStoreFloat3(&plPos2, XMVector3Transform({ -7.0f,4.0f,7.0f }, plRotation));

	Light_SetPointLightWorldByCount(
		0,
		plPos0,
		5.0f,
		XMFLOAT3(0.0f, 2.0f, 1.0f)
	);
	Light_SetPointLightWorldByCount(
		1,
		plPos1,
		3.0f,
		XMFLOAT3(1.0f, 0.0f, 1.0f)
	);
	Light_SetPointLightWorldByCount(
		2,
		plPos2,
		3.0f,
		XMFLOAT3(1.0f, 1.0f, 0.0f)
	);

	Sampler_SetFilterAnisotropic();

	XMMATRIX mtxWorld = XMMatrixIdentity();

	Light_SetSpecularWorld(Camera_GetPosition(), 1.0f,
		{ 0.1f,0.1f,0.1f,1.0f });

	ModelDraw(g_pModel, XMMatrixTranslation(-2.0f,1.0f,0.0f));

	Light_SetSpecularWorld(Camera_GetPosition(), 50.0f,
		{ 1.0f,0.9f,0.7f,1.0f });
	ModelDraw(g_pModel0,
		XMMatrixRotationAxis({0.0f,1.0f,0.0f},XMConvertToRadians(90))
		*XMMatrixTranslation(0.0f, 1.0f,3.0f));
	Cube_Draw(mtxWorld);

	Light_SetSpecularWorld(Camera_GetPosition(), 10.0f,{ 0.3f,0.3f,0.3f,1.0f });
	MeshField_Draw(mtxWorld);

	Camera_Debug();
}

void Game_Finalize()
{
	Camera_Finalize();
	Cube_Finalize();
	Grid_Finalize();
	Light_Finalize();
	Circle_DebugFinalize();
	Sampler_Finalize();
	ModelRelease(g_pModel);
	ModelRelease(g_pModel0);
}




