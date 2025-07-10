#include "score.h"

#include <algorithm>

#include "sprite.h"
#include "texture.h"



static constexpr int SCORE_FONT_SIZE = 32;// スコアのフォントサイズ

namespace 
{
	unsigned int g_Score = 0; // スコアを保持する変数
	unsigned int g_ViewScore = 0; // 表示用のスコア
	unsigned int g_CounterStop = 1; // スコアのカウンターを停止するための変数
	int g_Digit = 1; // スコアの桁数
	float g_X = 0.0f , g_Y = 0.0f; // スコアのY座標
	int g_ScoreTexId = -1; // スコアのテクスチャID

	

}

static void  drawNumber(float x, float y, int number);


void Score_Initialize(float x, float y, int digit)
{
	g_ViewScore = 0; // 表示用のスコアを初期化
	g_Digit = digit; // スコアの桁数を設定
	g_Score = 0; // スコアを初期化
	g_X = x; // スコアのX座標を初期化
	g_Y = y; // スコアのY座標を初期化

	for (int i = 0; i < digit; ++i)
	{
		g_CounterStop *= 10; // スコアのカウンターを桁数に応じて設定
	}

	g_CounterStop--;//

	g_ScoreTexId = Texture_LoadFromFile(L"resource/texture/game_num_font.png"); // スコアのテクスチャを読み込む
}

void Score_Finalize()
{
}

void Score_Draw()
{
	unsigned int temp = std::min(g_ViewScore, g_CounterStop); // スコアがカウンターを超えないように制限
	for (int i = 0; i < g_Digit; ++i)
	{
		int n = temp % 10; // 最下位の桁を取得
		float x = g_X + (g_Digit - i - 1) * SCORE_FONT_SIZE; // X座標を計算
		drawNumber(x, g_Y, n); // 数字を描画
		temp /= 10; // 次の桁へ移動
	}
}

void Score_Update()
{
	g_ViewScore = std::min(g_ViewScore + 1, g_Score); // スコアの表示用カウンターを更新
}

unsigned int Score_GetScore()
{
	return g_Score; // 現在のスコアを返す
}

void Score_AddScore(int score)
{
	g_ViewScore = g_Score;
	g_Score += score; // スコアを加算
}
void Score_Reset()
{
	g_Score = 0; // スコアをリセット
}

static void drawNumber(float x, float y, int number)
{
	Sprite_Draw(g_ScoreTexId,x,y,SCORE_FONT_SIZE*number,0, SCORE_FONT_SIZE, SCORE_FONT_SIZE);
}
