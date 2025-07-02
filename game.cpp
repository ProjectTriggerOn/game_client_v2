#include "game.h"

#include "player.h"
#include "bullet.h"
#include "sprite.h"
#include "texture.h"
#include "enemy_spawner.h"

static int bgTexId = -1;
static double g_Time = 0.0f;
static double g_SpawnTime = 0.0f;

void Game_Initialize()
{
	Enemy_Initialize();
	
	Player_Initialize({ 450 - 32.0f ,1600 - 64 });
	Bullet_Initialize(); // 弾の初期化
	bgTexId = Texture_LoadFromFile(L"resource/texture/bg.png"); // 背景テクスチャの読み込み

	EnemySpawner_Initialize(); // 敵スポーンの初期化
	EnemySpawner_Spawn({ 450 - 32.0f ,1600 - 64 },
		ENEMY_GREEN, 3.0f, 0.5f, 5); // 敵のスポーン設定
	EnemySpawner_Spawn({ 450 - 32.0f ,1600 - 64 },
		ENEMY_RED, 3.0f, 0.5f, 5); // 敵のスポーン設定

}

void Game_Update(double elapsed_time)
{
	EnemySpawner_Update(elapsed_time);
	Enemy_Update(elapsed_time,g_Time);
	Player_Update(elapsed_time);
	Bullet_Update(elapsed_time);
}

void Game_Draw()
{
	Sprite_Begin();
	Sprite_Draw(bgTexId, 0, 0, 900, 1600, 0, 0, 144, 256);
	Bullet_Draw();
	Player_Draw();
	Enemy_Draw();
}

void Game_Finalize()
{
	Bullet_Finalize();
	Player_Finalize();
	Enemy_Finalize();
	Texture_AllRelease(); // テクスチャの全てを解放
	EnemySpawner_Finalize(); // 敵スポーンの終了処理
}
