#ifndef BULLET_H
#define BULLET_H
#include <DirectXMath.h>

#include "collision.h"

static constexpr unsigned int MAX_BULLETS = 1024; // 最大弾数

void Bullet_Initialize();
	 
void Bullet_Update(double elapsed_time);
	 
void Bullet_Draw();
	 
void Bullet_Finalize();

void Bullet_Spawn(const DirectX::XMFLOAT2& position);

bool Bullet_IsEnable(int index);

Circle Bullet_GetCollision(int index);

void Bullet_Destroy(int index);



#endif