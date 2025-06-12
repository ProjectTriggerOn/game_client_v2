#pragma once
#ifndef SPRITE_H
#define SPRITE_H
#include <d3d11.h>

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);
void Sprite_Draw(float sx, float sy);

#endif	