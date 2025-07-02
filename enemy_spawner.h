#ifndef ENEMY_SPAWNER_H
#define ENEMY_SPAWNER_H
#include <DirectXMath.h>

#include "enemy.h"

void EnemySpawner_Initialize();

void EnemySpawner_Finalize();

void EnemySpawner_Update(double elapsed_time);

void EnemySpawner_Spawn(const DirectX::XMFLOAT2& position,EnemyTypeID id, 
	double spawn_time,double spawn_rate, int spawn_count);




#endif
