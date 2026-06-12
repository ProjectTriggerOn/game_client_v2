#pragma once
#ifndef SCENE_H
#define SCENE_H
#include <cstdint>

void Scene_Initialize();
void Scene_Finalize();

void Scene_Update(double elapsed_time);
void Scene_Draw();
void Scene_Refresh();


enum scene : std::uint8_t
{
	SCENE_TITLE,
	SCENE_GAME,
	SCENE_RESULT,
	SCENE_UI_TEST,   // isolated UI dev scene: no gameplay, no 3D, Ultralight UI only
	SCENE_MAX
};

void Scene_Change(scene scene);

// Call before Scene_Initialize (after main.cpp reads config) to set the boot
// scene. Calls after that are ignored.
void Scene_SetBootScene(scene scene);

void Restart_Game();



#endif