///Player Contorl///
#ifndef	PLAYER_H
#define PLAYER_H
#include <DirectXMath.h>

#include "collision.h"

void Player_Initialize(const DirectX::XMFLOAT2& position);

void Player_Update(double elapsed_time);

void Player_Draw();

void Player_Finalize();

bool Player_IsEnable();

Circle Player_GetCollision();

void Player_Destroy();

#endif

