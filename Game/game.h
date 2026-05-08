///ゲーム本体///

#ifndef GAME_H
#define GAME_H
#include "mouse.h"
#include "collision_world.h"


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

// Collision world accessor (for MockServer initialization)
CollisionWorld* Game_GetCollisionWorld();

// Local-player client-tick accessor (used by InputProducer to stamp cmd.tickId
// in the same domain as Player_Fps::m_InputHistory, so server's
// lastProcessedInputTick ack can be looked up)
uint32_t Game_GetClientTick();









#endif 