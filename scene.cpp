#include "scene.h"

#include "game.h"
#include "title.h"

namespace 
{
	scene g_CurrentScene = SCENE_GAME;// 現在のシーン
	scene g_NextScene = g_CurrentScene; // 次のシーン
}
void Scene_Initialize()
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Initialize();
		break;
	case SCENE_GAME:
		Game_Initialize();
		break;
	case SCENE_RESULT:

		break;
	default:
		break;
	}
}

void Scene_Finalize()
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Finalize();
		break;
	case SCENE_GAME:
		Game_Finalize();
		break;
	case SCENE_RESULT:

		break;
	default:
		break;
	}
}

void Scene_Update(double elapsed_time)
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Update(elapsed_time);
		break;
	case SCENE_GAME:
		Game_Update(elapsed_time);
		break;
	case SCENE_RESULT:

		break;
	default:
		break;
	}
}

void Scene_Draw()
{
	switch (g_CurrentScene)
	{
	case SCENE_TITLE:
		Title_Draw();
		break;
	case SCENE_GAME:

		Game_Draw();
		break;
	case SCENE_RESULT:

		break;
	default:
		break;
	}
}

void Scene_Refresh()
{
	if (g_CurrentScene != g_NextScene)
	{
		Scene_Finalize();
		g_CurrentScene = g_NextScene;
		Scene_Initialize();
	}
}

void Scene_Change(scene scene)
{
	g_NextScene = scene; // 現在のシーンを更新
}
