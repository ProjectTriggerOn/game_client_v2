#include "game.h"

#include "player.h"

void Game_Initialize()
{
	Player_Initialize({ 64.0f,450 - 32.0f });
}

void Game_Update(double elapsed_time)
{
	Player_Update(elapsed_time);
}

void Game_Draw()
{
	Player_Draw();
}

void Game_Finalize()
{
}
