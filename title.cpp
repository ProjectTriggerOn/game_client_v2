#include "title.h"
#include "fade.h"
#include "key_logger.h"
#include "scene.h"
#include "sprite.h"
#include "texture.h"

namespace 
{
	int g_TitleBG = -1; // タイトル画面の背景テクスチャID
	int g_TitleLogo = -1; // タイトルロゴのテクスチャID
}

void Title_Initialize()
{
	g_TitleBG = Texture_LoadFromFile(L"resource/texture/bg_v.png");
	g_TitleLogo = Texture_LoadFromFile(L"resource/texture/Title.png");
}

void Title_Finalize()
{
	Texture_Release(g_TitleBG);
	Texture_Release(g_TitleLogo);
}

void Title_Update(double )
{
	if (KeyLogger_IsTrigger(KK_ENTER))
	{
		Fade_Start(2.0, true); // フェードアウト開始
		
	}

	if (Fade_GetState() == FADE_STATE_FINISHED_OUT) {
		Scene_Change(SCENE_GAME); // ゲームシーンへ遷移
	}
}

void Title_Draw()
{
	Sprite_Draw(g_TitleBG, 0, 0, 1280, 960, 0, 0, 350, 400);
	Sprite_Draw(g_TitleLogo, 370, 300, 540, 50, 0, 0, 1514, 123);
}
