///ゲーム本体///

#ifndef GAME_H
#define GAME_H
#include "mouse.h"
#include "collision_world.h"

class PlayerFps; // local player lives in game.cpp's anonymous namespace


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

// True while gameplay needs a free cursor (debug TPS cam). PAUSE/SETTING go
// through UI::IsModalActive instead. Consumed by MousePolicy_Apply each frame —
// do not call Mouse_SetMode directly.
bool Game_WantsUICursor();

// True only while actively playing (SCENE_GAME + GameState PLAY). Consumed by
// InputProducer (zero input otherwise) and the HUD data push.
bool Game_IsGameplayActive();

// True while the server has the world frozen: the pre-match countdown or the
// result screen. NOT while waiting for players — that phase is a warm-up the
// server simulates normally. Consumed by InputProducer, which sends neutral
// movement/buttons so client-side prediction stays in step with a server that
// is ignoring them.
bool Game_IsMatchFrozen();

// Correction debug info
const char* Game_GetCorrectionMode();
float Game_GetCorrectionError();

// Collision world accessor (for MockServer initialization)
CollisionWorld* Game_GetCollisionWorld();

// Local-player accessor. g_PlayerFps itself lives in game.cpp's anonymous
// namespace (internal linkage), so cross-TU consumers go through this.
PlayerFps* Game_GetLocalPlayer();

// Local-player client-tick accessor (used by InputProducer to stamp cmd.tickId
// in the same domain as PlayerFps::m_InputHistory, so server's
// lastProcessedInputTick ack can be looked up)
uint32_t Game_GetClientTick();

// True while the local player's gameplay input is locked (dead, or during the
// short respawn fade). InputProducer sends neutral movement/buttons when set.
bool Game_IsPlayerInputLocked();

// Fractional server tick the local player is currently VIEWING (the interp-
// delayed remote-player render time mapped into tick space). outTick == 0
// means "no data yet" — the server must not rewind. Stamped into
// InputCmd.viewTick / viewTickFrac for lag compensation.
void Game_GetViewTick(uint32_t& outTick, float& outFrac);

// Debug readout: how many audio events the last snapshot derivation produced.
int Game_LastAudioEventCount();









#endif 