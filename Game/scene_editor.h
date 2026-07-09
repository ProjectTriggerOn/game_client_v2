#pragma once
#ifndef SCENE_EDITOR_H
#define SCENE_EDITOR_H

// In-engine level editor scene (enter via config.toml [debug] start_scene = "editor",
// typically set in the gitignored user_settings.toml).
//  - No gameplay runs and no networking (the main loop gates both to SCENE_GAME)
//  - Free-fly debug camera (Graphics/camera.cpp): WASD + arrows + Space/Ctrl
//  - Renders a loaded EditorMap: ground + box brushes + collider AABBs + spawns
//  - M2 is view + persistence only (no picking/gizmos/editing yet)

void SceneEditor_Initialize();
void SceneEditor_Finalize();
void SceneEditor_Update(double elapsed_time);
void SceneEditor_Draw();

#endif
