#pragma once
#ifndef PLAYER_H
#define PLAYER_H
#include <DirectXMath.h>

#include "collision.h"
using namespace DirectX;

void Player_Initialize(const DirectX::XMFLOAT3& positon, const DirectX::XMFLOAT3& front);
void Player_Finalize();

void Player_Update(double elapsed_time);

XMFLOAT3& Player_GetPosition();
XMFLOAT3& Player_GetFront();

AABB Player_GetAABB();

void Player_Draw();

#endif

