#pragma once
#ifndef SPRITE_H
#define SPRITE_H
#include <d3d11.h>
#include <DirectXMath.h>

void Sprite_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Sprite_Finalize(void);

void Sprite_Begin();

//テクスチャ全表示
void Sprite_Draw(int texid,float sx, float sy,
	const DirectX::XMFLOAT4& color = {1.0f,1.0f,1.0f,1.0f} );


//テクスチャ全表示（サイズ変更）
void Sprite_Draw(int texid,float sx, float sy, float sw, float sh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f });

//UVカット
void Sprite_Draw(int texid, float sx, float sy, int pixx,int pixy,int pixw,int pixh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f });

//UVカット（サイズ変更）
void Sprite_Draw(int texid, float sx, float sy, float sw, float sh,int pixx, int pixy, int pixw, int pixh,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f });
//UVカット（サイズ変更、回転）
void Sprite_Draw(int texid, float sx, float sy, float sw, float sh, int pixx, int pixy, int pixw, int pixh, float angle,
	const DirectX::XMFLOAT4& color = { 1.0f,1.0f,1.0f,1.0f });

#endif	