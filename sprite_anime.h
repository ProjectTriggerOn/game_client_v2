#pragma once
#ifndef SPRITE_ANIME_H
#define SPRITE_ANIME_H
#include <DirectXMath.h>

void SpriteAnime_Initialize();
void SpriteAnime_Finalize(void);

void SpriteAnime_Update(double elapsed_time);
void SpriteAnime_Draw(int playid,float x,float y,float dw,float dh);

int SpriteAnime_PatternRegister(int textrueId,int pattern_max,
	const DirectX::XMUINT2& pattern_size,
	const DirectX::XMUINT2& start_position,
	bool isLooped = true);

#endif
