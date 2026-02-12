///ゲーム本体///

#ifndef GAME_H
#define GAME_H
#include "mouse.h"


enum GameState
{
	TITLE,
	READY,
	COUNTDOWN,
	PLAY,
	PAUSE,
	SETTING,
	RESULT,
};

void Game_Initialize();

void Game_Update(double elapsed_time);

void Game_Draw();

void Game_Finalize();

void Game_SetState(GameState state);	

GameState Game_GetState();

// Correction debug info
const char* Game_GetCorrectionMode();
float Game_GetCorrectionError();









#endif 