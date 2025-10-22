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
}

void Game_Initialize()
{
	
	Camera_Initialize({7.785f,13.557f,-12.537f},
		{-0.291f,-0.777f,0.558f},
		{ 0.887f,0,0.462f },
		{-0.359f,0.629f,0.689f}
	);
	//Camera_Initialize();
	g_pModel = ModelLoad("resource/model/Tree.fbx");
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

	Light_SetAmbient({ 0.3f,0.3f,0.3f });
	Light_SetDirectionalWorld(
		{ 0.0f,-1.0f,0.0f,0.0f }, 
           { 1.0f,1.0f,1.0f,1.0f });

	Sampler_SetFilterAnisotropic();
	XMMATRIX mtxWorld = XMMatrixIdentity();
	Cube_Draw(mtxWorld);
	ModelDraw(g_pModel, mtxWorld);
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
	Circle_DebugFinalize();
	Sampler_Finalize();
	ModelRelease(g_pModel);
}




