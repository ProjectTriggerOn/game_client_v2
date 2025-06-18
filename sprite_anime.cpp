#include "sprite_anime.h"
#include "sprite.h"
#include "texture.h"
#include <DirectXMath.h>
using namespace DirectX;

struct AnimePatternData {
	int m_TextureId = -1;// テクスチャID
	int m_PatternMax = 0;//パ夕-ン数
	XMUINT2 m_StartPosition {0,0};//ア二メ一ションのスタ一卜座標
	XMUINT2 m_PatternSize = { 0,0 };//アニメーションパターンのサイズ
	bool m_IsLooped = true;//ル-プするか
};


struct AnimePlayData {
	int m_PatternId = -1;//アニメ-ションパタ-ンID
	int m_PatternNum = 0;//現在再生中のパ夕一ン番号
	double m_AccumulatedTime = 0.0;// 累積時間
};

static constexpr int ANIM_PATTERN_MAX = 128;
static AnimePatternData g_AnimePattern[ANIM_PATTERN_MAX]; // アニメーションパターンデータ
static constexpr int ANIM_PLAY_MAX = 256;
static AnimePlayData g_AnimePlayData[ANIM_PLAY_MAX]; // アニメーション再生データ



void SpriteAnime_Initialize()
{
	//g_TexId = Texture_LoadFromFile(L"kokosozai.png"); // テクスチャの読み込み
	g_AnimePattern[0].m_TextureId = Texture_LoadFromFile(L"kokosozai.png"); // テクスチャの読み込み
	g_AnimePattern[0].m_PatternMax = 13; // パターン数
	g_AnimePattern[0].m_StartPosition = { 0, 0 }; // アニメーションのスタート座標
	g_AnimePattern[0].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[0].m_PatternId = 0; // アニメーションパターンID

	g_AnimePattern[1].m_TextureId = g_AnimePattern[0].m_TextureId;
	g_AnimePattern[1].m_PatternMax = 13; // パターン数
	g_AnimePattern[1].m_StartPosition = { 0, 32 }; // アニメーションのスタート座標
	g_AnimePattern[1].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[1].m_PatternId = 1; // アニメーションパターンID

	g_AnimePattern[2].m_TextureId = g_AnimePattern[0].m_TextureId;
	g_AnimePattern[2].m_PatternMax = 6; // パターン数
	g_AnimePattern[2].m_StartPosition = { 0, 64 }; // アニメーションのスタート座標
	g_AnimePattern[2].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[2].m_PatternId = 2; // アニメーションパターンID

	g_AnimePattern[3].m_TextureId = g_AnimePattern[0].m_TextureId;
	g_AnimePattern[3].m_PatternMax = 8; // パターン数
	g_AnimePattern[3].m_StartPosition = { 0, 96 }; // アニメーションのスタート座標
	g_AnimePattern[3].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[3].m_PatternId = 3; // アニメーションパターンID

	g_AnimePattern[4].m_TextureId = g_AnimePattern[0].m_TextureId;
	g_AnimePattern[4].m_PatternMax = 15; // パターン数
	g_AnimePattern[4].m_StartPosition = { 0, 128 }; // アニメーションのスタート座標
	g_AnimePattern[4].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[4].m_PatternId = 4; // アニメーションパターンID

	g_AnimePattern[5].m_TextureId = g_AnimePattern[0].m_TextureId;
	g_AnimePattern[5].m_PatternMax = 4; // パターン数
	g_AnimePattern[5].m_StartPosition = { 64, 160 }; // アニメーションのスタート座標
	g_AnimePattern[5].m_PatternSize = { 32, 32 }; // アニメーションパターンのサイズ
	g_AnimePlayData[5].m_PatternId = 5; // アニメーションパターンID
}

void SpriteAnime_Finalize(void)
{
}

void SpriteAnime_Update(double elapsed_time)
{
	for (int i = 0;i < 6;i++) {
     	if (g_AnimePlayData[i].m_AccumulatedTime >= 0.1)
	{
		g_AnimePlayData[i].m_PatternNum = (g_AnimePlayData[i].m_PatternNum + 1) % 
			g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_PatternMax; // アニメーションパターンを更新
		g_AnimePlayData[i].m_AccumulatedTime = 0.0; // 経過時間をリセット
	}
	
	g_AnimePlayData[i].m_AccumulatedTime += elapsed_time; // 経過時間を更新
     	}
}

void SpriteAnime_Draw(int playid ,float x, float y, float dw, float dh)
{
	int i = playid;
	Sprite_Draw(g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_TextureId,
		x, y, dw, dh,
		g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_StartPosition.x
		+ g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_PatternSize.x
		* g_AnimePlayData[i].m_PatternNum,
		g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_StartPosition.y,
		g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_PatternSize.x,
		g_AnimePattern[g_AnimePlayData[i].m_PatternId].m_PatternSize.y);

}
