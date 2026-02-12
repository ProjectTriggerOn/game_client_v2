#pragma once
#ifndef SPRITE_ANIME_H
#define SPRITE_ANIME_H
#include <DirectXMath.h>

void SpriteAnime_Initialize();
void SpriteAnime_Finalize(void);

void SpriteAnime_Update(double elapsed_time);
void SpriteAnime_Draw(int playid,float x,float y,float dw,float dh);

int SpriteAnime_PatternRegister(int textrueId,int pattern_max, double second_per_pattern,
	const DirectX::XMUINT2& pattern_size,
	const DirectX::XMUINT2& start_position,
	bool isLooped = true, int pattern_col = 1);

int SpriteAnime_CreatePlayer(int anime_pattern_id);

bool SpriteAnime_IsStopped(int index);

void SpriteAnime_DestroyPlayer(int index);

#endif
