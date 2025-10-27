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

	Light_SetAmbient({ 0.4f,0.4f,0.4f });
	//XMVECTOR v{ 0.0f,-1.0f,0.0f };
	//v = XMVector3Normalize(v);
	//XMFLOAT4 dir;
	//XMStoreFloat4(&dir, v);
	//Light_SetDirectionalWorld(
	//	dir,
	//	{ 1.0f,0.9f,0.7f,1.0f },
	//	Camera_GetPosition()
	//	);
	Light_SetDirectionalWorld({ 0.0f,-1.0f,0.0f,0.0f }, { 1.0f,0.9f,0.7f,1.0f });//方向光


	Sampler_SetFilterAnisotropic();
	XMMATRIX mtxWorld = XMMatrixIdentity();


	Light_SetSpecularWorld(Camera_GetPosition(), 1.0f,
		{ 0.1f,0.1f,0.1f,1.0f });

	ModelDraw(g_pModel, XMMatrixTranslation(-2.0f,1.0f,0.0f));

	Light_SetSpecularWorld(Camera_GetPosition(), 50.0f,
		{ 1.0f,0.9f,0.7f,1.0f });
	ModelDraw(g_pModel0,
		XMMatrixRotationAxis({0.0f,1.0f,0.0f},
			XMConvertToRadians(90))
		*XMMatrixTranslation(0.0f, 1.0f,3.0f)
	);


	//ModelDraw(g_pModel0, mtxWorld);
	Cube_Draw(mtxWorld);
	MeshField_Draw(mtxWorld);
	Camera_Debug();
	//Grid_Draw();
	CircleD circle;
	circle.center = { 0.0f,0.0f,0.0f };
	circle.radius = 5.0f;
	Circle_DebugDraw(circle, { 1.0f,0.0f,0.0f,1.0f });

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




