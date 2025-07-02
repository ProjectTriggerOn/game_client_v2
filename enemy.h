#ifndef ENEMY_H
#define ENEMY_H
#include <DirectXMath.h>


void Enemy_Initialize();

void Enemy_Update(double elapsed_time,double game_time);

void Enemy_Draw();

enum EnemyTypeID : int {
	ENEMY_GREEN,
	ENEMY_RED,
};

void Enemy_Finalize();

void Enemy_Spawn(EnemyTypeID id, const DirectX::XMFLOAT2& position);

#endif


