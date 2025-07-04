#include "enemy_spawner.h"

struct EnemySpawner
{
	DirectX::XMFLOAT2 position; // スポーン位置
	EnemyTypeID id; // 敵のタイプID
	int maxCount; // スポーンする敵の最大数
	int spawnCount; // スポーンした敵の数
	double time; // スポーン時間
	double rate; // スポーンレート
	double lifeTime; // 敵の残り時間
	double spawnTime; // スポーン時間
	bool isCompleted; // スポーンが完了したかどうか
};

static constexpr unsigned int MAX_SPAWNERS = 100; // 最大スポーン数


namespace{
	EnemySpawner g_Spawners[MAX_SPAWNERS]{}; // スポーン情報の配列
	unsigned int g_SpawnerCount = 0; // 現在のスポーン数
	double g_Time;
}


void EnemySpawner_Initialize()
{
	g_SpawnerCount = 0; // スポーン数を初期化
	g_Time = 0.0; // 時間を初期化
}

void EnemySpawner_Finalize()
{
}

void EnemySpawner_Update(double elapsed_time)
{
	g_Time += elapsed_time; // 時間を更新
	for (unsigned int i = 0; i < g_SpawnerCount; ++i)
	{
		EnemySpawner& spawner = g_Spawners[i];

		if (spawner.isCompleted) continue; // スポーンが完了している場合はスキップ

		if (spawner.spawnTime > g_Time)break; // 如果生成时间小于当前时间，则跳出循环

		if (spawner.spawnCount == 0)
		{
			spawner.time = g_Time - spawner.rate - 0.00001;
		}

		if (g_Time - spawner.spawnTime >= spawner.rate) {
			Enemy_Spawn(spawner.id, spawner.position);
			spawner.spawnCount++; // スポーンカウントを増やす
			if (spawner.spawnCount >= spawner.maxCount)
			{
				spawner.isCompleted = true; // スポーンが完了
			}
			spawner.spawnTime = g_Time; // 次のスポーン時間を更新
		}
	}
}

void EnemySpawner_Spawn(const DirectX::XMFLOAT2& position, EnemyTypeID id, double spawn_time, double spawn_rate, int spawn_count)
{
	if (g_SpawnerCount >= MAX_SPAWNERS) return;
	EnemySpawner* spawner = &g_Spawners[g_SpawnerCount];
	spawner->position = position; // スポーン位置を設定
	spawner->id = id; // 敵のタイプIDを設定
	spawner->maxCount = spawn_count; // 最大スポーン数を設定
	spawner->spawnCount = 0; // スポーンした敵の数を初期化
	spawner->time = spawn_time; // スポーン時間を設定
	spawner->spawnTime = 0.0; // 敵の残り時間を初期化
	spawner->rate = spawn_rate; // スポーンレートを設定
	spawner->isCompleted = false; // スポーンが完了していない状態に設定
	g_SpawnerCount++; // スポーン数を増やす
}
