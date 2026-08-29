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

// --- Bridge entry points (UI/ui_bridge.cpp, EDITOR_ENABLED only) -------------
// Called from inside a JS callback, so they only mutate editor state: no D3D
// work, no page navigation, no window poking.

void SceneEditor_SetTool(int tool);      // 0 Select, 1 Move, 2 Rotate, 3 Scale
void SceneEditor_SetSnap(bool on);
void SceneEditor_Undo();
void SceneEditor_Redo();
void SceneEditor_DeleteSelection();
void SceneEditor_Save();
void SceneEditor_Reload();

// Forces PublishSelection/PublishStatus to re-push next Update even if their
// content hasn't changed since last frame. Needed because OnDOMReady's dirty-flag
// re-arm (UI/ui_manager.cpp) can fire before router.js has finished injecting
// #ed-tools/#ed-counts/#ed-xform into the (still empty) page — that flush is lost
// and, since both publishers dedupe on content, nothing re-sends it on its own.
// Call once the panel markup actually exists (editor.js PageEditor.onEnter).
void SceneEditor_RequestRepublish();

// field: "pos" | "rot" (DEGREES) | "scale" | "cmin" | "cmax".
// Commits ONE undoable command through the same stack the gizmo uses.
void SceneEditor_SetTransform(const char* field, float x, float y, float z);
void SceneEditor_SetColliderGround(bool isGround);

#endif
