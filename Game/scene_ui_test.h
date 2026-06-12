#pragma once
#ifndef SCENE_UI_TEST_H
#define SCENE_UI_TEST_H

// Isolated UI development scene (enter via config.toml [debug] start_scene = "ui_test"):
//  - No gameplay runs (Game_Update is never called; keyboard/mouse never drive
//    the player or the camera)
//  - No 3D scene is drawn; the backdrop is Direct3D_Clear's solid color
//  - Mouse is pinned to ABSOLUTE + visible; all input goes to Ultralight
//  - F1/F2 switch between pages (Slice B/C verification shortcut, moved here
//    from main.cpp)

void UITest_Initialize();
void UITest_Finalize();
void UITest_Update(double elapsed_time);
void UITest_Draw();

#endif
