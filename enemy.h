#ifndef ENEMY_H
#define ENEMY_H
#include <DirectXMath.h>

#include "collision.h"


static constexpr unsigned int MAX_ENEMIES = 1024; // 最大敵数

void Enemy_Initialize();

void Enemy_Update(double elapsed_time,double game_time);

void Enemy_Draw();

enum EnemyTypeID : int {
	ENEMY_GREEN,
	ENEMY_RED,
};

void Enemy_Finalize();

void Enemy_Spawn(EnemyTypeID id, const DirectX::XMFLOAT2& position);

bool Enemy_IsEnable(int index);

Circle Enemy_GetCollision(int index);

void Enemy_Destroy(int index);

#endif


