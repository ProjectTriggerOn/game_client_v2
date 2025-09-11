#include "game.h"
#include "cube.h"
#include "shader.h"
#include "grid.h"
#include "camera.h"

namespace{

}

void Game_Initialize()
{
	Camera_Initialize();
}

void Game_Update(double elapsed_time)
{
	
	Cube_Update(elapsed_time);
}

void Game_Draw()
{
	Camera_Update();
	//Shader_Begin();
	Cube_Draw();
	Grid_Draw();
}

void Game_Finalize()
{
	Camera_Finalize();
	Cube_Finalize();
	Grid_Finalize();
}




