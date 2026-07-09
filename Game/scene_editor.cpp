//=============================================================================
// scene_editor.cpp
//
// In-engine level editor scene. M2: load + render + save an EditorMap.
// (Camera/render/save are filled in by later M2 tasks; picking/gizmos are M3+.)
//=============================================================================
// Includes editor_map.h -> map_io.h (std::fopen); silence C4996 under /sdl.
#define _CRT_SECURE_NO_WARNINGS

#include "scene_editor.h"

void SceneEditor_Initialize()
{
    // Filled in Task 3 (camera + texture) and Task 4 (map load).
}

void SceneEditor_Finalize()
{
    // Filled in Task 3 (Camera_Finalize).
}

void SceneEditor_Update([[maybe_unused]] double elapsed_time)
{
    // Filled in Task 3 (camera) and Task 5 (save/reload hotkeys).
}

void SceneEditor_Draw()
{
    // Filled in Task 3 (ground/lights) and Task 4 (map objects).
    // Until then the backdrop is Direct3D_Clear's color (like the ui_test scene).
}
