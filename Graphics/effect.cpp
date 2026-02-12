#include "effect.h"

#include <iso646.h>

#include "texture.h"
#include "sprite_anime.h"

#include "Audio.h"
using namespace DirectX;

struct Effect
{
	XMFLOAT2 position; // エフェクトの位置
	//XMFLOAT2 velocity; // エフェクトの速度
	int spriteAnimeId; // テクスチャID
	//double lifetime; // エフェクトの残り時間
	bool isEnable; // エフェクトが有効かどうか
};


namespace{
	 constexpr unsigned int MAX_EFFECTS = 256; // 最大エフェクト数
	 Effect g_Effects[MAX_EFFECTS]{}; // エフェクトの配列
	 int g_EffectTexId = -1; // エフェクトのアニメーションパターンID
	 int g_EffectAnimeId = -1; // エフェクトのテクスチャID
	 int g_EffectSoundId = -1; // エフェクトのサウンドID
	}

void Effect_Initialize(void)
{
	g_EffectTexId = Texture_LoadFromFile(L"resource/texture/Explosion.png"); // エフェクトのテクスチャIDを読み込む
	g_EffectSoundId = LoadAudio("resource/audio/se.wav"); // エフェクトのサウンドIDを読み込む
	for (Effect& e : g_Effects)
	{

		e.isEnable = false; // 初期化時は全てのエフェクトを無効にする
	}

	g_EffectAnimeId = SpriteAnime_PatternRegister(g_EffectTexId, 16, 0.05, { 256,256 }, { 0,0 },
		false, 4); // エフェクトのアニメーションパターンを登録

}

void Effect_Finalize(void)
{
	UnloadAudio(g_EffectSoundId); // エフェクトのサウンドをアンロード
}

void Effect_Update(double)
{
	for (Effect& e : g_Effects)
	{
		if (!e.isEnable) continue; // エフェクトが無効な場合はスキップ
		if (SpriteAnime_IsStopped(e.spriteAnimeId))
		{
			SpriteAnime_DestroyPlayer(e.spriteAnimeId);
			e.isEnable = false; // アニメーションが停止したらエフェクトを無効にする
		}
	}
}

void Effect_Draw()
{
	for (Effect& e : g_Effects)
	{
		if (!e.isEnable) continue; // エフェクトが無効な場合はスキップ
		SpriteAnime_Draw(e.spriteAnimeId, e.position.x, e.position.y, 64.0f, 64.0f); // エフェクトの描画
	}
}

void Effect_Create(const DirectX::XMFLOAT2 position)
{
	for (Effect& e : g_Effects)
	{
		if (e.isEnable) continue;
		//空き領域発見

		e.position = position; // 弾の位置を設定
		e.isEnable = true; // エフェクトを有効にする
		e.spriteAnimeId = SpriteAnime_CreatePlayer(g_EffectAnimeId); // アニメーションを登録
		PlayAudio(g_EffectSoundId);
		break;
	}
}
