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
#include "editor_camera.h"
#include "cube.h"
#include "mesh_field.h"
#include "texture.h"
#include "light.h"
#include "collision.h"
#include "sampler.h"
#include "sprite.h"
#include "direct3d.h"
#include "key_logger.h"
#include "ms_logger.h"

#include <DirectXMath.h>
#include <cmath>
#include <Windows.h>

using namespace DirectX;

namespace {
    editor::EditorMap g_Map;
    int               g_CubeTexId = -1;
    const char* const kEditorSavePath = "resource/maps/_editor_save.map";

    int  g_LastMouseX = 0;
    int  g_LastMouseY = 0;
    int  g_LastWheel  = 0;
    bool g_MouseInit  = false;

    // AABB over everything drawable, for "frame all" (A). Falls back to a unit box.
    void MapWorldBounds(DirectX::XMFLOAT3& mn, DirectX::XMFLOAT3& mx)
    {
        bool any = false;
        mn = { 1e9f, 1e9f, 1e9f };
        mx = { -1e9f, -1e9f, -1e9f };
        auto acc = [&](float x, float y, float z) {
            mn.x = (x < mn.x) ? x : mn.x; mn.y = (y < mn.y) ? y : mn.y; mn.z = (z < mn.z) ? z : mn.z;
            mx.x = (x > mx.x) ? x : mx.x; mx.y = (y > mx.y) ? y : mx.y; mx.z = (z > mx.z) ? z : mx.z;
            any = true;
        };
        for (const auto& b : g_Map.boxes) {
            acc(b.pos.x - b.scale.x * 0.5f, b.pos.y - b.scale.y * 0.5f, b.pos.z - b.scale.z * 0.5f);
            acc(b.pos.x + b.scale.x * 0.5f, b.pos.y + b.scale.y * 0.5f, b.pos.z + b.scale.z * 0.5f);
        }
        for (const auto& c : g_Map.colliders) { acc(c.min.x, c.min.y, c.min.z); acc(c.max.x, c.max.y, c.max.z); }
        if (!any) { mn = { -5, 0, -5 }; mx = { 5, 4, 5 }; }
    }
}

void SceneEditor_Initialize()
{
    // Create the shared view/projection constant buffers (slots b1/b2) that
    // Camera_SetMatrixToShader binds in SceneEditor_Draw. Game_Initialize does
    // this for the game scene; the editor boot path never runs it, so without
    // this call the buffers stay null and Draw feeds null CBs to the 3D VS.
    Camera_Initialize();

    EditorCamera_Init({ 0.0f, 1.0f, 0.0f },  // pivot at map center
                      22.0f,                  // distance
                      0.0f,                   // yaw
                      0.6f);                  // pitch (look down ~34 deg)

    g_CubeTexId = Texture_LoadFromFile(L"resource/texture/stone_001.jpg");

    if (!editor::EditorMap_Load("resource/maps/default.map", g_Map))
        OutputDebugStringA("[EDITOR] WARNING: failed to load resource/maps/default.map\n");
}

void SceneEditor_Finalize()
{
    // Pair the Camera_Initialize() from SceneEditor_Initialize (mirrors
    // Game_Finalize) so the constant buffers / debug text don't leak on exit.
    Camera_Finalize();
}

void SceneEditor_Update([[maybe_unused]] double elapsed_time)
{
    // Frame-to-frame mouse delta (cursor stays absolute+visible; see spec §8.1).
    int mx = MSLogger_GetX(), my = MSLogger_GetY();
    if (!g_MouseInit) { g_LastMouseX = mx; g_LastMouseY = my; g_LastWheel = MSLogger_GetScrollWheelValue(); g_MouseInit = true; }
    float dx = static_cast<float>(mx - g_LastMouseX);
    float dy = static_cast<float>(my - g_LastMouseY);
    g_LastMouseX = mx; g_LastMouseY = my;

    int wheel = MSLogger_GetScrollWheelValue();
    float wheelSteps = static_cast<float>(wheel - g_LastWheel) / 120.0f;   // WHEEL_DELTA = 120
    g_LastWheel = wheel;

    const bool alt = KeyLogger_IsPressed(KK_LEFTALT);

    // §8.3 arbitration: Alt+button => camera. (Pick/gizmo branches added in later tasks.)
    if (alt) {
        if (MSLogger_IsPressed(MBT_LEFT))   EditorCamera_Tumble(dx, dy);
        if (MSLogger_IsPressed(MBT_MIDDLE)) EditorCamera_Track(dx, dy);
        if (MSLogger_IsPressed(MBT_RIGHT))  EditorCamera_Dolly(dx * 0.05f);
    }
    if (wheelSteps != 0.0f) EditorCamera_Dolly(wheelSteps);   // wheel dollies (Maya), no Alt needed

    // Framing: F = frame selection (no selection yet -> whole map), A = frame all.
    if (KeyLogger_IsTrigger(KK_F) || KeyLogger_IsTrigger(KK_A)) {
        DirectX::XMFLOAT3 bmn, bmx; MapWorldBounds(bmn, bmx);
        EditorCamera_FrameBounds(bmn, bmx);
    }

    if (KeyLogger_IsTrigger(KK_F9)) {
        bool ok = editor::EditorMap_Save(kEditorSavePath, g_Map);
        OutputDebugStringA(ok ? "[EDITOR] saved resource/maps/_editor_save.map\n"
                              : "[EDITOR] save FAILED (is resource/maps/ writable?)\n");
    }
    if (KeyLogger_IsTrigger(KK_F10)) {
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

    XMFLOAT4X4 v4 = EditorCamera_GetView();
    XMFLOAT4X4 p4 = EditorCamera_GetProj();
    XMMATRIX view = XMLoadFloat4x4(&v4);
    XMMATRIX proj = XMLoadFloat4x4(&p4);
    Camera_SetMatrixToShader(view, proj);

    XMFLOAT4 dir;
    XMStoreFloat4(&dir, XMVector3Normalize(XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f)));
    Light_SetDirectionalWorld(dir, { 1.0f, 1.0f, 1.0f, 1.0f });
    Light_SetSpecularWorld(EditorCamera_GetEye(), 4.0f, { 0.3f, 0.3f, 0.3f, 1.0f });

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
