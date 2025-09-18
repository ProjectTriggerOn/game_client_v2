#include "game.h"
#include "cube.h"
#include "shader.h"
#include "grid.h"
#include "camera.h"
#include "circle.h"
#include "key_logger.h"
using namespace DirectX;

namespace{
	XMFLOAT3 g_CubePosition{};
	XMFLOAT3 g_CubeVelocity{};
}

void Game_Initialize()
{
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
	cube_position = XMLoadFloat3(&Camera_GetFront()) * 10.0f * elapsed_time + cube_position;
	XMStoreFloat3(&g_CubePosition, cube_position);
}

void Game_Draw()
{
	
	//Shader_Begin();
	//XMMATRIX mtxWorld = XMMatrixTranslationFromVector(XMLoadFloat3(&Camera_GetFront()) * 10.0F);

	XMMATRIX mtxWorld = XMMatrixIdentity();
	Cube_Draw(mtxWorld);
	Camera_Debug();
	Grid_Draw();
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
}




