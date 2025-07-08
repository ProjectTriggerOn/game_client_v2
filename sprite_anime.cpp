#include "sprite_anime.h"
#include "sprite.h"
#include "texture.h"
#include <DirectXMath.h>
using namespace DirectX;

struct AnimePatternData {
	int m_TextureId = -1;// テクスチャID
	int m_PatternMax = 0;//パ夕-ン数
	int m_PatternCol = 1;//パターンの列数（横方向）
	XMUINT2 m_StartPosition {0,0};//ア二メ一ションのスタ一卜座標
	XMUINT2 m_PatternSize = { 0,0 };//アニメーションパターンのサイズ
	bool m_IsLooped = true;//ル-プするか
	double m_second_per_pattern = 0.1; // パターンごとの秒数（デフォルトは0.1秒）
};


struct AnimePlayData {
	int m_PatternId = -1;//アニメ-ションパタ-ンID
	int m_PatternNum = 0;//現在再生中のパ夕一ン番号
	double m_AccumulatedTime = 0.0;// 累積時間
	bool m_isStopped = false; // 再生が停止しているかどうか
};

static constexpr int ANIM_PATTERN_MAX = 128;
static AnimePatternData g_AnimePattern[ANIM_PATTERN_MAX]; // アニメーションパターンデータ
static constexpr int ANIM_PLAY_MAX = 256;
static AnimePlayData g_AnimePlayData[ANIM_PLAY_MAX]; // アニメーション再生データ



void SpriteAnime_Initialize()
{
	for (AnimePatternData& data : g_AnimePattern) {
		data.m_TextureId = -1; // 初期化

	}

	for (AnimePlayData& data : g_AnimePlayData) {
		data.m_PatternId = -1; // 初期化
		data.m_isStopped = false; // 再生が停止しているかどうかを初期化
	}
}

void SpriteAnime_Finalize(void)
{
}

void SpriteAnime_Update(double elapsed_time)
{
	for (int i = 0; i < ANIM_PLAY_MAX; i++) {
		if (g_AnimePlayData[i].m_PatternId < 0) continue; // 使用されていないプレイヤーはスキップ
		AnimePatternData* pAnmPtrnData = &g_AnimePattern[g_AnimePlayData[i].m_PatternId];

		if (g_AnimePlayData[i].m_AccumulatedTime >= pAnmPtrnData->m_second_per_pattern) {

			g_AnimePlayData[i].m_PatternNum++; // パターン番号を更新
			
			if (g_AnimePlayData[i].m_PatternNum >= pAnmPtrnData->m_PatternMax) {

				if (pAnmPtrnData->m_IsLooped) {

					g_AnimePlayData[i].m_PatternNum = 0; // ループする場合はパターン番号をリセット

				}
				else {

					g_AnimePlayData[i].m_PatternNum = pAnmPtrnData->m_PatternMax - 1; // ループしない場合は最後のパターンに留める
					g_AnimePlayData[i].m_isStopped = true; // 再生を停止
				}
				
			}
			g_AnimePlayData[i].m_AccumulatedTime -= pAnmPtrnData->m_second_per_pattern; // 累積時間から経過時間を引く
			
		}
		g_AnimePlayData[i].m_AccumulatedTime += elapsed_time; // 経過時間を更新
	}
}
	void SpriteAnime_Draw(int playid, float x, float y, float dw, float dh) {

		int anm_ptrn_id = g_AnimePlayData[playid].m_PatternId;

		AnimePatternData* pAnmPtrnData = &g_AnimePattern[anm_ptrn_id];

		Sprite_Draw(pAnmPtrnData->m_TextureId,
			x, y, dw, dh,
			pAnmPtrnData->m_StartPosition.x + pAnmPtrnData->m_PatternSize.x 
			* (g_AnimePlayData[playid].m_PatternNum % pAnmPtrnData->m_PatternCol),
			pAnmPtrnData->m_StartPosition.y + pAnmPtrnData->m_PatternSize.y 
			* (g_AnimePlayData[playid].m_PatternNum / pAnmPtrnData->m_PatternCol),
			pAnmPtrnData->m_PatternSize.x,
			pAnmPtrnData->m_PatternSize.y);

	}

	int SpriteAnime_PatternRegister(int textrueId, int pattern_max, double second_per_pattern,
		const DirectX::XMUINT2& pattern_size, const DirectX::XMUINT2& start_position, bool isLooped, int pattern_col)
	{
		for (int i = 0; i < ANIM_PATTERN_MAX; i++) {

			if (g_AnimePattern[i].m_TextureId >= 0)continue; // 既に使用中のパターンはスキップ

			g_AnimePattern[i].m_TextureId = textrueId; // テクスチャIDを設定
			g_AnimePattern[i].m_PatternMax = pattern_max; // パターン数を設定
			g_AnimePattern[i].m_second_per_pattern = second_per_pattern; // パターンごとの秒数を設定
			g_AnimePattern[i].m_PatternCol = pattern_col; // パターンの列数を設定
			g_AnimePattern[i].m_StartPosition = start_position; // アニメーションのスタート座標を設定
			g_AnimePattern[i].m_PatternSize = pattern_size; // アニメーションパターンのサイズを設定
			g_AnimePattern[i].m_IsLooped = isLooped; // ループ設定を保存
			return i; // 登録したパターンのIDを返す
		}
		return -1;
	}

	int SpriteAnime_CreatePlayer(int anime_pattern_id)
	{
		for (int i = 0; i < ANIM_PLAY_MAX; i++) {
			if (g_AnimePlayData[i].m_PatternId >= 0)continue; // 既に使用中のプレイヤーはスキップ
			g_AnimePlayData[i].m_PatternId = anime_pattern_id; // アニメーションパターンIDを設定
			g_AnimePlayData[i].m_PatternNum = 0; // パターン番号を初期化
			g_AnimePlayData[i].m_AccumulatedTime = 0.0; // 累積時間を初期化
			g_AnimePlayData[i].m_isStopped = false; // 再生が停止しているかどうかを初期化
			return i; // 登録したプレイヤーのIDを返す
		}	
		return -1;
	}

	bool SpriteAnime_IsStopped(int index)
	{
		return g_AnimePlayData[index].m_isStopped; // 再生が停止しているかどうかを返す
	}

	void SpriteAnime_DestroyPlayer(int index)
	{
		g_AnimePlayData[index].m_PatternId = -1; // アニメーションパターンIDを無効化
	}

