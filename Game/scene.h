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

scene Scene_GetCurrent();

// Call before Scene_Initialize (after main.cpp reads config) to set the boot
// scene. Calls after that are ignored.
void Scene_SetBootScene(scene scene);

void Restart_Game();

//-----------------------------------------------------------------------------
// Generic masked scene transition.
//
// Covers the screen with the loading curtain (black fade-in), swaps to `target`
// while fully black so the heavy Scene_Initialize/Game_Initialize hitch lands on
// a black frame, holds a minimum loading time, then reveals the new scene (black
// fade-out). Use this instead of Scene_Change for user-facing scene switches
// (PLAY, return-to-title). No-op if a transition is already in progress.
//
// `ready` (optional, EXTENSION POINT): a predicate polled during the hold/loading
// phase. The curtain lifts only once it returns true AND a minimum hold has elapsed
// (a safety cap forces a reveal if it never does). Pass nullptr (the default) for a
// purely time-based transition — correct today, since Game_Initialize is synchronous
// and finishes in the swap frame before the hold even starts. Pass a real predicate
// when the target scene loads ASYNCHRONOUSLY (asset streaming, server connect, lobby
// join) so the curtain stays up exactly until loading completes. It is a plain
// function pointer polling global state (e.g. a free `bool Assets_Ready()`); upgrade
// to std::function if per-transition captured state is ever needed.
//-----------------------------------------------------------------------------
void SceneTransition_To(scene target, bool (*ready)() = nullptr);

// Advance the transition state machine; call once per frame from the main loop.
void SceneTransition_Update(double elapsed_time);

// True while a masked transition is in progress (curtain up). Gameplay input is
// neutralized during this window (see input_producer.cpp).
bool SceneTransition_IsActive();



#endif