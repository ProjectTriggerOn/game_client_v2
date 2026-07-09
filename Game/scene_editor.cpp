//=============================================================================
// scene_editor.cpp
//
// In-engine level editor scene. M2: load + render + save an EditorMap.
// (Camera/render/save are filled in by later M2 tasks; picking/gizmos are M3+.)
//=============================================================================
// Includes editor_map.h -> map_io.h (std::fopen); silence C4996 under /sdl.
#define _CRT_SECURE_NO_WARNINGS

#include "scene_editor.h"
#include "editor_map.h"

#include "camera.h"
#include "cube.h"
#include "mesh_field.h"
#include "texture.h"
#include "light.h"
#include "collision.h"
#include "sampler.h"
#include "sprite.h"
#include "direct3d.h"
#include "key_logger.h"

#include <DirectXMath.h>
#include <cmath>
#include <Windows.h>

using namespace DirectX;

namespace {
    editor::EditorMap g_Map;
    int               g_CubeTexId = -1;
}

void SceneEditor_Initialize()
{
    // Free-fly camera positioned above/behind the map, looking toward +Z and
    // slightly down so the whole pillar layout (z in [-7,7]) is in frame.
    Camera_Initialize({ 0.0f, 10.0f, -18.0f },   // position
                      { 0.0f, -0.35f, 1.0f },    // front (normalized inside)
                      { 1.0f, 0.0f, 0.0f },      // right
                      { 0.0f, 1.0f, 0.0f });     // up

    g_CubeTexId = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");
}

void SceneEditor_Finalize()
{
    Camera_Finalize();
}

void SceneEditor_Update(double elapsed_time)
{
    // Free-fly navigation: WASD move, arrows look, Space/Ctrl up/down, Z/X fov.
    Camera_Update(elapsed_time);
}

void SceneEditor_Draw()
{
    Sampler_SetFilterAnisotropic();
    Light_SetAmbient({ 0.5f, 0.5f, 0.5f });

    XMFLOAT4X4 v4 = Camera_GetViewMatrix();
    XMFLOAT4X4 p4 = Camera_GetPerspectiveMatrix();
    XMMATRIX view = XMLoadFloat4x4(&v4);
    XMMATRIX proj = XMLoadFloat4x4(&p4);
    Camera_SetMatrixToShader(view, proj);

    XMFLOAT4 dir;
    XMStoreFloat4(&dir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f)));
    Light_SetDirectionalWorld(dir, { 1.0f, 1.0f, 1.0f, 1.0f });
    Light_SetSpecularWorld(Camera_GetPosition(), 4.0f, { 0.3f, 0.3f, 0.3f, 1.0f });

    // Ground plane (same Y offset as the game scene).
    MeshField_Draw(XMMatrixTranslation(0.0f, -1.0f, 0.0f));
}
