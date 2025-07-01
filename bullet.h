#ifndef BULLET_H
#define BULLET_H
#include <DirectXMath.h>


void Bullet_Initialize(const DirectX::XMFLOAT2& position);
	 
void Bullet_Update(double elapsed_time);
	 
void Bullet_Draw();
	 
void Bullet_Finalize();

void Bullet_Spawn(const DirectX::XMFLOAT2& position);

#endif