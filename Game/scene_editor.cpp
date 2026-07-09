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
    const char* const kEditorSavePath = "resource/maps/_editor_save.map";
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

    if (!editor::EditorMap_Load("resource/maps/default.map", g_Map))
        OutputDebugStringA("[EDITOR] WARNING: failed to load resource/maps/default.map\n");
}

void SceneEditor_Finalize()
{
    Camera_Finalize();
}

void SceneEditor_Update(double elapsed_time)
{
    // Free-fly navigation: WASD move, arrows look, Space/Ctrl up/down, Z/X fov.
    Camera_Update(elapsed_time);

    // M2 persistence probe (no editor UI yet): F9 saves the in-memory map to a
    // scratch file, F10 reloads it. Editing (M3) and UI buttons (M4) come later.
    if (KeyLogger_IsTrigger(KK_F9))
    {
        bool ok = editor::EditorMap_Save(kEditorSavePath, g_Map);
        OutputDebugStringA(ok ? "[EDITOR] saved resource/maps/_editor_save.map\n"
                              : "[EDITOR] save FAILED (is resource/maps/ writable?)\n");
    }
    if (KeyLogger_IsTrigger(KK_F10))
    {
        if (editor::EditorMap_Load(kEditorSavePath, g_Map))
            OutputDebugStringA("[EDITOR] reloaded resource/maps/_editor_save.map\n");
        else
            OutputDebugStringA("[EDITOR] reload FAILED (save with F9 first)\n");
    }
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

    // Box brushes: scaled cubes (mirrors Map_Draw's transform).
    Cube_SetUVMode(CUBE_UV_PER_FACE);
    for (const auto& b : g_Map.boxes)
    {
        XMMATRIX w = XMMatrixScaling(b.scale.x, b.scale.y, b.scale.z)
                   * XMMatrixTranslation(b.pos.x, b.pos.y, b.pos.z);
        Cube_Draw(g_CubeTexId, w);
    }

    // Colliders + spawns as debug geometry. Depth off so they read through the
    // box brushes. Collision_DebugDraw overwrites CB0, so restore the 2D ortho
    // projection with Sprite_Begin afterward (mirrors game.cpp).
    Direct3D_SetDepthEnable(false);
    Collision_DebugSetViewProj(view * proj);

    for (const auto& c : g_Map.colliders)
    {
        AABB a{ { c.min.x, c.min.y, c.min.z }, { c.max.x, c.max.y, c.max.z } };
        XMFLOAT4 col = c.isGround ? XMFLOAT4{ 0.0f, 0.5f, 1.0f, 1.0f }    // blue ground
                                  : XMFLOAT4{ 1.0f, 0.5f, 0.0f, 1.0f };   // orange walls
        Collision_DebugDraw(a, col);
    }

    for (const auto& s : g_Map.spawns)
    {
        XMFLOAT4 col = (s.team == mapio::TEAM_RED) ? XMFLOAT4{ 1.0f, 0.2f, 0.2f, 1.0f }
                                                   : XMFLOAT4{ 0.2f, 0.4f, 1.0f, 1.0f };
        XMFLOAT3 base = s.pos;
        XMFLOAT3 top  = { base.x, base.y + 2.0f, base.z };
        Collision_DebugDrawLine(base, top, col);                          // vertical post
        XMFLOAT3 head = { base.x + std::sin(s.yaw) * 1.5f, base.y + 0.1f,
                          base.z + std::cos(s.yaw) * 1.5f };
        Collision_DebugDrawLine(base, head, col);                         // facing arrow (+Z at yaw 0)
    }

    Direct3D_SetDepthEnable(true);
    Sprite_Begin();
}
