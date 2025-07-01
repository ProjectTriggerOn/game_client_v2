#include "game.h"

#include "player.h"
#include "bullet.h"
#include "sprite.h"
#include "texture.h"

static int bgTexId = -1;

void Game_Initialize()
{
	Player_Initialize({ 450 - 32.0f ,1600 - 64 });
	Bullet_Initialize({ 64.0f,450 - 32.0f }); // 弾の初期化
	bgTexId = Texture_LoadFromFile(L"resource/texture/bg.png"); // 背景テクスチャの読み込み
}

void Game_Update(double elapsed_time)
{
	Player_Update(elapsed_time);
	Bullet_Update(elapsed_time);
}

void Game_Draw()
{
	Sprite_Draw(bgTexId, 0, 0, 900, 1600, 0, 0, 144, 256);
	Bullet_Draw();
	Player_Draw();
}

void Game_Finalize()
{
}
