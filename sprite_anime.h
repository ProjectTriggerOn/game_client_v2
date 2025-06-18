#pragma once
#ifndef SPRITE_ANIME_H
#define SPRITE_ANIME_H

void SpriteAnime_Initialize();
void SpriteAnime_Finalize(void);

void SpriteAnime_Update(double elapsed_time);
void SpriteAnime_Draw(int playid,float x,float y,float dw,float dh);

#endif
