#include "sprite_anime.h"
#include "sprite.h"
#include "texture.h"

static int g_PatternMax = 13; // アニメーションのパターン数
static int g_PatternIndex = 0; // 現在のアニメーションパターンインデックス
static int g_TexId = -1; // テクスチャID

void SpriteAnime_Initialize()
{
	g_TexId = Texture_LoadFromFile(L"kokosozai.png"); // テクスチャの読み込み
}

void SpriteAnime_Finalize(void)
{
}

void SpriteAnime_Update()
{
	g_PatternIndex = (g_PatternIndex + 1) % g_PatternMax; // アニメーションパターンを更新
}

void SpriteAnime_Draw()
{
	Sprite_Draw(g_TexId, 700.0f, 64.0f, 450, 900, 32 * g_PatternIndex, 0, 32, 64);

}
