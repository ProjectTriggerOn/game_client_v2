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
using namespace DirectX;

namespace{
	XMFLOAT3 g_CubePosition{};
	XMFLOAT3 g_CubeVelocity{};
}

void Game_Initialize()
{
	
	//Camera_Initialize({3.0f,2.45f,-3.0f},{-0.6f,-0.45f,0.66f},{-0.75,0.0,0.66});
	Camera_Initialize();

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
		{ 0.0f,0.0f,1.0f,0.0f }, 
           { 1.0f,1.0f,1.0f,1.0f });

	Sampler_SetFilterAnisotropic();
	XMMATRIX mtxWorld = XMMatrixIdentity();
	Cube_Draw(mtxWorld);
	//MeshField_Draw(mtxWorld);
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
}




