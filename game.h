///ゲーム本体///

#ifndef GAME_H
#define GAME_H
#include "mouse.h"

void Game_Initialize();

void Game_Update(double elapsed_time);

void Game_Draw();

void Game_Finalize();

bool isMouseLeftTrigger(const Mouse_State& ms );








#endif 