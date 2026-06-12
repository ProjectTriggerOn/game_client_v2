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

// True while gameplay needs a free cursor (settings overlay / debug TPS cam).
// Consumed by MousePolicy_Apply each frame — do not call Mouse_SetMode directly.
bool Game_WantsUICursor();

// Correction debug info
const char* Game_GetCorrectionMode();
float Game_GetCorrectionError();

// Collision world accessor (for MockServer initialization)
CollisionWorld* Game_GetCollisionWorld();

// Local-player client-tick accessor (used by InputProducer to stamp cmd.tickId
// in the same domain as PlayerFps::m_InputHistory, so server's
// lastProcessedInputTick ack can be looked up)
uint32_t Game_GetClientTick();

// Fractional server tick the local player is currently VIEWING (the interp-
// delayed remote-player render time mapped into tick space). outTick == 0
// means "no data yet" — the server must not rewind. Stamped into
// InputCmd.viewTick / viewTickFrac for lag compensation.
void Game_GetViewTick(uint32_t& outTick, float& outFrac);









#endif 