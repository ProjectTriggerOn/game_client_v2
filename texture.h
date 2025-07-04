#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H
#include <d3d11.h>


void Texture_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Texture_Finalize(void);


// テクスチャの読み込み
//戻り値：管理番号。読み込めなかった場合は-1
int Texture_LoadFromFile(const wchar_t* filename);

void Texture_AllRelease();

void Texture_Set(int texid);

unsigned int Texture_GetWidth(int texid);
unsigned int Texture_GetHeight(int texid);

void Texture_Release(int texid);


#endif
