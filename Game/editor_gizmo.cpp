//=============================================================================
// editor_gizmo.cpp — manipulators (spec §3.4, §8.2).
//=============================================================================
#include "editor_gizmo.h"
#include "editor_pick.h"
#include <cmath>

using namespace DirectX;

namespace {
    // Drag state, shared by translate/scale/rotate.
    GizmoAxis g_Axis = GizmoAxis::None;
    XMFLOAT3  g_DragStartHit {};    // world point where the drag began (on the axis/plane)
    XMFLOAT3  g_AxisDir {};         // unit world axis being dragged
    XMFLOAT3  g_PlanePt {};         // drag plane point (object center)
    XMFLOAT3  g_PlaneN  {};         // drag plane normal

    XMVECTOR AxisVec(GizmoAxis a) {
        switch (a) {
            case GizmoAxis::X: return XMVectorSet(1, 0, 0, 0);
            case GizmoAxis::Y: return XMVectorSet(0, 1, 0, 0);
            case GizmoAxis::Z: return XMVectorSet(0, 0, 1, 0);
            default:           return XMVectorSet(0, 1, 0, 0);
        }
    }

    // Distance (pixels) from point P to segment AB in screen space.
    float DistToSeg(const XMFLOAT2& p, const XMFLOAT2& a, const XMFLOAT2& b) {
        float vx = b.x - a.x, vy = b.y - a.y;
        float wx = p.x - a.x, wy = p.y - a.y;
        float len2 = vx * vx + vy * vy;
        float t = (len2 > 1e-6f) ? (wx * vx + wy * vy) / len2 : 0.0f;
        t = (t < 0) ? 0 : (t > 1 ? 1 : t);
        float cx = a.x + t * vx, cy = a.y + t * vy;
        float dx = p.x - cx, dy = p.y - cy;
        return std::sqrt(dx * dx + dy * dy);
    }

    constexpr float PICK_PIXELS = 12.0f;
}

float EditorGizmo_HandleLen(const XMFLOAT3& center, const XMFLOAT3& eye) {
    XMVECTOR c = XMLoadFloat3(&center), e = XMLoadFloat3(&eye);
    float dist = XMVectorGetX(XMVector3Length(e - c));
    return dist * 0.15f;   // ~constant on-screen size
}

void EditorGizmo_DrawTranslate(const XMFLOAT3& center, const XMFLOAT3& eye, GizmoAxis hot, const XMMATRIX&) {
    float L = EditorGizmo_HandleLen(center, eye);
    XMFLOAT3 x = { center.x + L, center.y, center.z };
    XMFLOAT3 y = { center.x, center.y + L, center.z };
    XMFLOAT3 z = { center.x, center.y, center.z + L };
    XMFLOAT4 cx = (hot == GizmoAxis::X) ? XMFLOAT4{1,1,1,1} : XMFLOAT4{1,0.15f,0.15f,1};
    XMFLOAT4 cy = (hot == GizmoAxis::Y) ? XMFLOAT4{1,1,1,1} : XMFLOAT4{0.15f,1,0.15f,1};
    XMFLOAT4 cz = (hot == GizmoAxis::Z) ? XMFLOAT4{1,1,1,1} : XMFLOAT4{0.3f,0.5f,1,1};
    Collision_DebugDrawLine(center, x, cx);
    Collision_DebugDrawLine(center, y, cy);
    Collision_DebugDrawLine(center, z, cz);
}

GizmoAxis EditorGizmo_PickAxis(const XMFLOAT3& center, const XMFLOAT3& eye, int mouseX, int mouseY,
                               const XMFLOAT4X4& view, const XMFLOAT4X4& proj) {
    float L = EditorGizmo_HandleLen(center, eye);
    XMFLOAT2 p = { (float)mouseX, (float)mouseY };
    XMFLOAT2 c2, e2;
    if (!EditorPick_WorldToScreen(center, view, proj, c2)) return GizmoAxis::None;
    struct AR { GizmoAxis ax; XMFLOAT3 end; };
    AR arr[3] = {
        { GizmoAxis::X, { center.x + L, center.y, center.z } },
        { GizmoAxis::Y, { center.x, center.y + L, center.z } },
        { GizmoAxis::Z, { center.x, center.y, center.z + L } },
    };
    GizmoAxis best = GizmoAxis::None; float bestD = PICK_PIXELS;
    for (auto& a : arr) {
        if (!EditorPick_WorldToScreen(a.end, view, proj, e2)) continue;
        float d = DistToSeg(p, c2, e2);
        if (d < bestD) { bestD = d; best = a.ax; }
    }
    return best;
}

void EditorGizmo_BeginDrag(GizmoAxis axis, const XMFLOAT3& center, const Ray& ray, const XMFLOAT3& eye) {
    g_Axis   = axis;
    g_PlanePt = center;
    XMVECTOR ax = AxisVec(axis);
    XMStoreFloat3(&g_AxisDir, ax);
    // Drag plane: contains the axis, faces the camera as much as possible.
    XMVECTOR toEye = XMVector3Normalize(XMLoadFloat3(&eye) - XMLoadFloat3(&center));
    XMVECTOR n = XMVector3Cross(ax, XMVector3Cross(toEye, ax));
    if (XMVectorGetX(XMVector3LengthSq(n)) < 1e-6f) n = toEye;    // axis ~ parallel to view: use view plane
    n = XMVector3Normalize(n);
    XMStoreFloat3(&g_PlaneN, n);
    XMFLOAT3 hit;
    if (EditorPick_RayPlane(ray, g_PlanePt, g_PlaneN, hit)) g_DragStartHit = hit;
    else g_DragStartHit = center;
}

XMFLOAT3 EditorGizmo_DragTranslate(const Ray& ray) {
    XMFLOAT3 hit;
    if (!EditorPick_RayPlane(ray, g_PlanePt, g_PlaneN, hit)) return { 0, 0, 0 };
    XMVECTOR delta = XMLoadFloat3(&hit) - XMLoadFloat3(&g_DragStartHit);
    XMVECTOR ax = XMLoadFloat3(&g_AxisDir);
    float along = XMVectorGetX(XMVector3Dot(delta, ax));
    XMFLOAT3 out; XMStoreFloat3(&out, ax * along);
    return out;   // world offset along the active axis
}
