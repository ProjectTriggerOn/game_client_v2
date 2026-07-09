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
#include "editor_pick.h"
#include "editor_command.h"
#include "editor_gizmo.h"
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
#include <memory>
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

    using SelKind = editor::ElemKind;
    struct Selection { bool has = false; SelKind kind = SelKind::Box; int index = -1; };
    Selection g_Sel;
    editor::CommandStack g_Cmds;

    // AABB of a selectable, for pick + highlight + gizmo pivot.
    bool SelectableAABB(SelKind kind, int i, AABB& out) {
        if (kind == SelKind::Box && i >= 0 && i < (int)g_Map.boxes.size()) {
            const auto& b = g_Map.boxes[i];
            out = { { b.pos.x - b.scale.x*0.5f, b.pos.y - b.scale.y*0.5f, b.pos.z - b.scale.z*0.5f },
                    { b.pos.x + b.scale.x*0.5f, b.pos.y + b.scale.y*0.5f, b.pos.z + b.scale.z*0.5f } };
            return true;
        }
        if (kind == SelKind::Collider && i >= 0 && i < (int)g_Map.colliders.size()) {
            const auto& c = g_Map.colliders[i];
            out = { { c.min.x, c.min.y, c.min.z }, { c.max.x, c.max.y, c.max.z } };
            return true;
        }
        return false;   // Model added in Task 6
    }

    DirectX::XMFLOAT3 AABBCenter(const AABB& a) {
        return { (a.min.x+a.max.x)*0.5f, (a.min.y+a.max.y)*0.5f, (a.min.z+a.max.z)*0.5f };
    }

    // Ray-pick the nearest selectable. Returns {has=false} on miss.
    Selection PickNearest(const Ray& r) {
        Selection best; float bestT = 1e30f;
        auto test = [&](SelKind kind, int count) {
            for (int i = 0; i < count; i++) {
                AABB a; if (!SelectableAABB(kind, i, a)) continue;
                float t; if (EditorPick_RayAABB(r, a, t) && t < bestT) { bestT = t; best = { true, kind, i }; }
            }
        };
        test(SelKind::Box, (int)g_Map.boxes.size());
        test(SelKind::Collider, (int)g_Map.colliders.size());
        return best;
    }

    // §8.2 tool state (Q/W/E/R). Move gizmo lands in Task 4; rotate/scale in T5.
    enum class Tool { Select, Move, Rotate, Scale };
    Tool      g_Tool = Tool::Select;
    bool      g_Dragging = false;
    GizmoAxis g_HotAxis = GizmoAxis::None;
    // Drag snapshot for building the command on release:
    DirectX::XMFLOAT3 g_DragBeforeA {}, g_DragBeforeB {};   // Box/Model: A=pos or scale; Collider: A=min,B=max

    float SnapTo(float v, float step) { return step > 0.0f ? std::round(v / step) * step : v; }
    DirectX::XMFLOAT3 SelectionCenter() {
        AABB a; if (g_Sel.has && SelectableAABB(g_Sel.kind, g_Sel.index, a)) return AABBCenter(a);
        return { 0, 0, 0 };
    }
    DirectX::XMFLOAT3 SelectionMovePos() {   // the value MoveCommand tracks
        if (g_Sel.kind == SelKind::Box)      return g_Map.boxes[g_Sel.index].pos;
        if (g_Sel.kind == SelKind::Model)    return g_Map.models[g_Sel.index].pos;
        return g_Map.colliders[g_Sel.index].min;   // Collider tracked by min
    }
    // g_Sel.index still in range for its list? A mid-drag Delete/Undo/Redo can
    // erase the dragged element and clear g_Sel.has while g_Dragging is still
    // true — guard before indexing g_Map[...] to avoid an OOB write.
    bool SelectionIndexValid() {
        if (!g_Sel.has || g_Sel.index < 0) return false;
        if (g_Sel.kind == SelKind::Box)   return g_Sel.index < (int)g_Map.boxes.size();
        if (g_Sel.kind == SelKind::Model) return g_Sel.index < (int)g_Map.models.size();
        return g_Sel.index < (int)g_Map.colliders.size();
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

    // g_Map/g_Cmds/g_Sel are namespace-scope statics that outlive a scene
    // teardown, so re-entering the editor reloads a fresh g_Map while any
    // command/selection from the previous session would still hold indices into
    // the old map. Start every editor session with an empty stack + no selection.
    g_Cmds.Clear();
    g_Sel = Selection{};
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
    // The editor runs in MOUSE_POSITION_MODE_ABSOLUTE (MousePolicy_Apply forces
    // every non-SCENE_GAME scene into absolute/UI mode), so the cursor lives in
    // MSLogger's MODE_UI slot — the MODE_GAME x/y are zeroed each frame. Read the
    // *UI accessors (as ui_widget.cpp does) so the pick ray gets the real cursor.
    int mx = MSLogger_GetXUI(), my = MSLogger_GetYUI();
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

    // §8.2 tools: Q select / W move / E rotate / R scale. Ignored mid-drag: the
    // live-apply has already mutated g_Map, and the release path only records a
    // command for the tool that started the drag (g_Tool==Move). Letting the tool
    // change here would orphan that mutation (no command => cannot be undone).
    if (!g_Dragging) {
        if (KeyLogger_IsTrigger(KK_Q)) g_Tool = Tool::Select;
        if (KeyLogger_IsTrigger(KK_W)) g_Tool = Tool::Move;
        if (KeyLogger_IsTrigger(KK_E)) g_Tool = Tool::Rotate;
        if (KeyLogger_IsTrigger(KK_R)) g_Tool = Tool::Scale;
    }

    // §8.3 arbitration rule 2: no Alt => gizmo/pick. Begin a drag on a handle,
    // continue it (live-apply so the user sees the move), release => push ONE
    // command with before/after, else ray-pick to select/deselect. The editor
    // runs in absolute/UI mouse mode (MousePolicy_Apply), so the live LMB state
    // lives in MSLogger's UI slot — read *UI accessors (matching the T2 pick).
    if (!alt) {
        Ray ray = EditorPick_ScreenRay(mx, my, EditorCamera_GetView(), EditorCamera_GetProj());
        const bool ctrlSnap = !KeyLogger_IsPressed(KK_LEFTCONTROL);   // Ctrl held = free (no snap)

        if (g_Dragging) {
            if (!SelectionIndexValid()) {
                // Selection was invalidated mid-drag (Delete/Undo/Redo). Abandon
                // the drag without applying or recording a command — the live
                // mutation up to this point stays under the command that caused it.
                g_Dragging = false; g_HotAxis = GizmoAxis::None;
            } else if (MSLogger_IsPressedUI(MBT_LEFT)) {
                if (g_Tool == Tool::Move) {
                    XMFLOAT3 off = EditorGizmo_DragTranslate(ray);
                    XMFLOAT3 np = { g_DragBeforeA.x + off.x, g_DragBeforeA.y + off.y, g_DragBeforeA.z + off.z };
                    if (ctrlSnap) { np.x = SnapTo(np.x, 0.5f); np.y = SnapTo(np.y, 0.5f); np.z = SnapTo(np.z, 0.5f); }
                    // live-apply so the user sees it; command captures before/after on release
                    if (g_Sel.kind == SelKind::Box)        g_Map.boxes[g_Sel.index].pos = np;
                    else if (g_Sel.kind == SelKind::Model)  g_Map.models[g_Sel.index].pos = np;
                    else { auto& c = g_Map.colliders[g_Sel.index];
                           XMFLOAT3 d = { np.x-c.min.x, np.y-c.min.y, np.z-c.min.z };
                           c.min = np; c.max = { c.max.x+d.x, c.max.y+d.y, c.max.z+d.z }; }
                }
                else if (g_Tool == Tool::Scale) {
                    XMFLOAT3 f = EditorGizmo_DragScale(ray);
                    if (g_Sel.kind == SelKind::Box)
                        g_Map.boxes[g_Sel.index].scale = { g_DragBeforeA.x*f.x, g_DragBeforeA.y*f.y, g_DragBeforeA.z*f.z };
                    else if (g_Sel.kind == SelKind::Model)
                        g_Map.models[g_Sel.index].scale = { g_DragBeforeA.x*f.x, g_DragBeforeA.y*f.y, g_DragBeforeA.z*f.z };
                    else { // Collider: scale extents about center
                        XMFLOAT3 ctr = { (g_DragBeforeA.x+g_DragBeforeB.x)*0.5f, (g_DragBeforeA.y+g_DragBeforeB.y)*0.5f, (g_DragBeforeA.z+g_DragBeforeB.z)*0.5f };
                        XMFLOAT3 half = { (g_DragBeforeB.x-g_DragBeforeA.x)*0.5f*f.x, (g_DragBeforeB.y-g_DragBeforeA.y)*0.5f*f.y, (g_DragBeforeB.z-g_DragBeforeA.z)*0.5f*f.z };
                        g_Map.colliders[g_Sel.index].min = { ctr.x-half.x, ctr.y-half.y, ctr.z-half.z };
                        g_Map.colliders[g_Sel.index].max = { ctr.x+half.x, ctr.y+half.y, ctr.z+half.z };
                    }
                }
                else if (g_Tool == Tool::Rotate) {
                    float ang = EditorGizmo_DragRotate(ray, SelectionCenter());
                    const bool snap = !KeyLogger_IsPressed(KK_LEFTCONTROL);
                    if (snap) ang = SnapTo(ang, XM_PI/12.0f);   // 15 degrees
                    XMFLOAT3 e = g_DragBeforeA;
                    if (g_HotAxis==GizmoAxis::X) e.x += ang; else if (g_HotAxis==GizmoAxis::Y) e.y += ang; else e.z += ang;
                    if (g_Sel.kind == SelKind::Box)   g_Map.boxes[g_Sel.index].rotEuler = e;
                    else if (g_Sel.kind == SelKind::Model) g_Map.models[g_Sel.index].rotEuler = e;
                }
            } else {
                // Release: record an undoable command from before/after.
                if (g_Tool == Tool::Move) {
                    XMFLOAT3 after = SelectionMovePos();
                    g_Cmds.Execute(std::make_unique<editor::MoveCommand>(g_Map, g_Sel.kind, g_Sel.index, g_DragBeforeA, after));
                }
                else if (g_Tool == Tool::Scale) {
                    if (g_Sel.kind == SelKind::Collider)
                        g_Cmds.Execute(std::make_unique<editor::ScaleCommand>(g_Map, g_Sel.kind, g_Sel.index, g_DragBeforeA, g_DragBeforeB, g_Map.colliders[g_Sel.index].min, g_Map.colliders[g_Sel.index].max));
                    else {
                        XMFLOAT3 after = (g_Sel.kind==SelKind::Box) ? g_Map.boxes[g_Sel.index].scale : g_Map.models[g_Sel.index].scale;
                        g_Cmds.Execute(std::make_unique<editor::ScaleCommand>(g_Map, g_Sel.kind, g_Sel.index, g_DragBeforeA, XMFLOAT3{0,0,0}, after, XMFLOAT3{0,0,0}));
                    }
                }
                else if (g_Tool == Tool::Rotate) {
                    XMFLOAT3 after = (g_Sel.kind==SelKind::Box) ? g_Map.boxes[g_Sel.index].rotEuler : g_Map.models[g_Sel.index].rotEuler;
                    g_Cmds.Execute(std::make_unique<editor::RotateCommand>(g_Map, g_Sel.index, g_Sel.kind, g_DragBeforeA, after));
                }
                g_Dragging = false;
                g_HotAxis = GizmoAxis::None;
            }
        } else if (MSLogger_IsTriggerUI(MBT_LEFT)) {
            GizmoAxis axis = GizmoAxis::None;
            if (g_Sel.has && g_Tool == Tool::Move)
                axis = EditorGizmo_PickAxis(SelectionCenter(), EditorCamera_GetEye(), mx, my, EditorCamera_GetView(), EditorCamera_GetProj());
            else if (g_Sel.has && g_Tool == Tool::Scale)
                axis = EditorGizmo_PickAxis(SelectionCenter(), EditorCamera_GetEye(), mx, my, EditorCamera_GetView(), EditorCamera_GetProj());
            else if (g_Sel.has && g_Tool == Tool::Rotate)
                axis = EditorGizmo_PickRing(SelectionCenter(), EditorCamera_GetEye(), mx, my, EditorCamera_GetView(), EditorCamera_GetProj());

            if (axis != GizmoAxis::None) {
                g_Dragging = true; g_HotAxis = axis;
                if (g_Tool == Tool::Move) {
                    g_DragBeforeA = SelectionMovePos();
                    EditorGizmo_BeginDrag(axis, SelectionCenter(), ray, EditorCamera_GetEye());
                } else if (g_Tool == Tool::Scale) {
                    if (g_Sel.kind == SelKind::Collider) { g_DragBeforeA = g_Map.colliders[g_Sel.index].min; g_DragBeforeB = g_Map.colliders[g_Sel.index].max; }
                    else if (g_Sel.kind == SelKind::Box)  g_DragBeforeA = g_Map.boxes[g_Sel.index].scale;
                    else                                  g_DragBeforeA = g_Map.models[g_Sel.index].scale;
                    EditorGizmo_BeginDrag(axis, SelectionCenter(), ray, EditorCamera_GetEye());
                } else { // Rotate
                    if (g_Sel.kind == SelKind::Box)   g_DragBeforeA = g_Map.boxes[g_Sel.index].rotEuler;
                    else if (g_Sel.kind == SelKind::Model) g_DragBeforeA = g_Map.models[g_Sel.index].rotEuler;
                    else { g_HotAxis = GizmoAxis::None; g_Dragging = false; /* colliders don't rotate */ }
                    if (g_Dragging) EditorGizmo_BeginRingDrag(axis, SelectionCenter(), ray);
                }
            } else {
                g_Sel = PickNearest(ray);   // click selects / deselects
            }
        }
    }

    // §8.2 delete + undo/redo. Clearing selection avoids a stale index pointing
    // at a shifted/removed element after the structural change (safe v1 choice).
    const bool ctrl = KeyLogger_IsPressed(KK_LEFTCONTROL);
    if (KeyLogger_IsTrigger(KK_DELETE) && g_Sel.has) {
        g_Cmds.Execute(std::make_unique<editor::DeleteCommand>(g_Map, g_Sel.kind, g_Sel.index));
        g_Sel.has = false;
    }
    if (ctrl && KeyLogger_IsTrigger(KK_Z)) { g_Cmds.Undo(); g_Sel.has = false; }
    if (ctrl && KeyLogger_IsTrigger(KK_Y)) { g_Cmds.Redo(); g_Sel.has = false; }

    if (wheelSteps != 0.0f) EditorCamera_Dolly(wheelSteps);   // wheel dollies (Maya), no Alt needed

    // Framing: F frames the selection (whole map if nothing is selected), A frames all.
    if (KeyLogger_IsTrigger(KK_F)) {
        AABB a;
        if (g_Sel.has && SelectableAABB(g_Sel.kind, g_Sel.index, a))
            EditorCamera_FrameBounds(a.min, a.max);
        else { DirectX::XMFLOAT3 mn, mx2; MapWorldBounds(mn, mx2); EditorCamera_FrameBounds(mn, mx2); }
    }
    if (KeyLogger_IsTrigger(KK_A)) { DirectX::XMFLOAT3 mn, mx2; MapWorldBounds(mn, mx2); EditorCamera_FrameBounds(mn, mx2); }

    if (KeyLogger_IsTrigger(KK_F9)) {
        bool ok = editor::EditorMap_Save(kEditorSavePath, g_Map);
        OutputDebugStringA(ok ? "[EDITOR] saved resource/maps/_editor_save.map\n"
                              : "[EDITOR] save FAILED (is resource/maps/ writable?)\n");
    }
    if (KeyLogger_IsTrigger(KK_F10)) {
        if (editor::EditorMap_Load(kEditorSavePath, g_Map)) {
            // g_Map was replaced wholesale: any buffered command holds indices
            // into the old (differently-sized) vectors, and g_Sel may point past
            // the new end. Both are stale now — undo/redo or Delete against them
            // would insert/erase/read out of bounds. Drop both.
            g_Cmds.Clear();
            g_Sel = Selection{};
            OutputDebugStringA("[EDITOR] reloaded resource/maps/_editor_save.map\n");
        } else {
            OutputDebugStringA("[EDITOR] reload FAILED (save with F9 first)\n");
        }
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
                   * XMMatrixRotationRollPitchYaw(b.rotEuler.x, b.rotEuler.y, b.rotEuler.z)
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

    // Selection highlight (depth still off so it reads through the box brushes).
    if (g_Sel.has) {
        AABB a;
        if (SelectableAABB(g_Sel.kind, g_Sel.index, a))
            Collision_DebugDraw(a, { 1.0f, 1.0f, 0.0f, 1.0f });   // selection = yellow
    }

    // Manipulator for the active tool (drawn with the same depth-off debug lines).
    if (g_Sel.has && g_Tool != Tool::Select) {
        XMFLOAT3 gc = SelectionCenter();
        XMFLOAT3 eye = EditorCamera_GetEye();
        GizmoAxis hot = g_Dragging ? g_HotAxis : GizmoAxis::None;
        if (g_Tool == Tool::Move)   EditorGizmo_DrawTranslate(gc, eye, hot, view * proj);
        if (g_Tool == Tool::Scale)  EditorGizmo_DrawScale(gc, eye, hot, view * proj);
        if (g_Tool == Tool::Rotate) EditorGizmo_DrawRotate(gc, eye, hot, view * proj);
    }

    Direct3D_SetDepthEnable(true);
    Sprite_Begin();
}
