///Player Contorl///
#ifndef	PLAYER_H
#define PLAYER_H


#include "collision.h"

void Player_Initialize(const DirectX::XMFLOAT2& position);

void Player_Update(double elapsed_time);

void Player_Draw();

void Player_Finalize();

bool Player_IsEnable();

Circle Player_GetCollision();

void Player_Destroy();

float Player_GetVelocityX();

void Player_TakeDamage(int damage);

int Player_GetLife();
#endif

