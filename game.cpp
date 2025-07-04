#include "game.h"

#include "player.h"
#include "bullet.h"
#include "sprite.h"
#include "texture.h"
#include "enemy_spawner.h"

namespace{
	int bgTexId = -1;
	int uiTexId = -1;
	double g_Time = 0.0f;
	double g_SpawnTime = 0.0f;
}

void Game_Initialize()
{
	Enemy_Initialize();
	
	Player_Initialize({ 640 - 32.0f ,720 - 64 });
	Bullet_Initialize(); // 弾の初期化
	bgTexId = Texture_LoadFromFile(L"resource/texture/bg.png"); // 背景テクスチャの読み込み
	uiTexId = Texture_LoadFromFile(L"resource/texture/black.png"); // UIテクスチャの読み込み

	EnemySpawner_Initialize(); // 敵スポーンの初期化
	EnemySpawner_Spawn({ 640 - 32.0f ,50 },
		ENEMY_GREEN, 3.0f, 0.3f, 25); // 敵のスポーン設定
	EnemySpawner_Spawn({ 640 - 32.0f ,50},
		ENEMY_RED, 4.0f, 0.5f, 25); // 敵のスポーン設定

}

void Game_Update(double elapsed_time)
{
	EnemySpawner_Update(elapsed_time);
	Enemy_Update(elapsed_time,g_Time);
	Player_Update(elapsed_time);
	Bullet_Update(elapsed_time);
	CollisionDetect_Bullet_Enemy();
	CollisionDetect_Player_Enemy();
}

void Game_Draw()
{
	Sprite_Begin();
	Sprite_Draw(bgTexId, 370, 0, 540, 960, 0, 0, 144, 256);
	Bullet_Draw();
	Player_Draw();
	Enemy_Draw();
	Sprite_Draw(uiTexId, 0, 0, 370, 960, 0, 0, 2, 2);
	Sprite_Draw(uiTexId, 910, 0, 370, 960, 0, 0, 2, 2);
}

void Game_Finalize()
{
	Bullet_Finalize();
	Player_Finalize();
	Enemy_Finalize();
	Texture_AllRelease(); // テクスチャの全てを解放
	EnemySpawner_Finalize(); // 敵スポーンの終了処理
}

void CollisionDetect_Bullet_Enemy()
{
	for (int bi = 0; bi < MAX_BULLETS; ++bi)
	{
		if (!Bullet_IsEnable(bi)) continue;
		Circle bulletCollision = Bullet_GetCollision(bi);
		for (int ei = 0; ei < MAX_ENEMIES; ++ei)
		{
			if (!Enemy_IsEnable(ei)) continue;
			Circle enemyCollision = Enemy_GetCollision(ei);
			if (Collision_OverlapCircleCircle(bulletCollision,enemyCollision))
			{
				Bullet_Destroy(bi); // 弾を破壊
				Enemy_Destroy(ei); // 敵を破壊
				break; // 一つの弾が一つの敵にしか当たらないようにする
			}
		}
	}
}

void CollisionDetect_Player_Enemy()
{
	if (!Player_IsEnable()) return; // プレイヤーが無効な場合は何もしない
	for (int ei = 0; ei < MAX_ENEMIES; ++ei)
	{
		if (!Enemy_IsEnable(ei)) continue;
		Circle enemyCollision = Enemy_GetCollision(ei);
		Circle playerCollision = Player_GetCollision();
		if (Collision_OverlapCircleCircle(enemyCollision, playerCollision))
		{
			Player_Destroy(); // プレイヤーを破壊
			Enemy_Destroy(ei); // 敵を破壊
			break; // 一つの敵が一つのプレイヤーにしか当たらないようにする
		}
	}
}
