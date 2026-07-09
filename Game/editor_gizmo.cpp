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

void EditorGizmo_DrawScale(const XMFLOAT3& center, const XMFLOAT3& eye, GizmoAxis hot, const XMMATRIX&) {
    float L = EditorGizmo_HandleLen(center, eye);
    XMFLOAT3 x = { center.x + L, center.y, center.z };
    XMFLOAT3 y = { center.x, center.y + L, center.z };
    XMFLOAT3 z = { center.x, center.y, center.z + L };
    XMFLOAT4 cx = (hot==GizmoAxis::X)?XMFLOAT4{1,1,1,1}:XMFLOAT4{1,0.15f,0.15f,1};
    XMFLOAT4 cy = (hot==GizmoAxis::Y)?XMFLOAT4{1,1,1,1}:XMFLOAT4{0.15f,1,0.15f,1};
    XMFLOAT4 cz = (hot==GizmoAxis::Z)?XMFLOAT4{1,1,1,1}:XMFLOAT4{0.3f,0.5f,1,1};
    Collision_DebugDrawLine(center, x, cx);
    Collision_DebugDrawLine(center, y, cy);
    Collision_DebugDrawLine(center, z, cz);
    // Center uniform-scale marker: a small box outline.
    float s = L * 0.12f;
    XMFLOAT4 cu = (hot==GizmoAxis::Uniform)?XMFLOAT4{1,1,1,1}:XMFLOAT4{1,1,0.2f,1};
    XMFLOAT3 a{center.x-s,center.y-s,center.z}, b{center.x+s,center.y-s,center.z};
    XMFLOAT3 c{center.x+s,center.y+s,center.z}, d{center.x-s,center.y+s,center.z};
    Collision_DebugDrawLine(a,b,cu); Collision_DebugDrawLine(b,c,cu);
    Collision_DebugDrawLine(c,d,cu); Collision_DebugDrawLine(d,a,cu);
}

void EditorGizmo_DrawRotate(const XMFLOAT3& center, const XMFLOAT3& eye, GizmoAxis hot, const XMMATRIX&) {
    float L = EditorGizmo_HandleLen(center, eye);
    const int SEG = 32;
    auto ring = [&](GizmoAxis ax, const XMFLOAT4& col) {
        XMFLOAT3 prev{};
        for (int i = 0; i <= SEG; i++) {
            float t = (float)i / SEG * XM_2PI;
            float a = std::cos(t) * L, b = std::sin(t) * L;
            XMFLOAT3 p;
            if (ax == GizmoAxis::X)      p = { center.x, center.y + a, center.z + b };
            else if (ax == GizmoAxis::Y) p = { center.x + a, center.y, center.z + b };
            else                         p = { center.x + a, center.y + b, center.z };
            if (i > 0) Collision_DebugDrawLine(prev, p, col);
            prev = p;
        }
    };
    ring(GizmoAxis::X, (hot==GizmoAxis::X)?XMFLOAT4{1,1,1,1}:XMFLOAT4{1,0.15f,0.15f,1});
    ring(GizmoAxis::Y, (hot==GizmoAxis::Y)?XMFLOAT4{1,1,1,1}:XMFLOAT4{0.15f,1,0.15f,1});
    ring(GizmoAxis::Z, (hot==GizmoAxis::Z)?XMFLOAT4{1,1,1,1}:XMFLOAT4{0.3f,0.5f,1,1});
}

GizmoAxis EditorGizmo_PickRing(const XMFLOAT3& center, const XMFLOAT3& eye, int mouseX, int mouseY,
                               const XMFLOAT4X4& view, const XMFLOAT4X4& proj) {
    float L = EditorGizmo_HandleLen(center, eye);
    XMFLOAT2 p = { (float)mouseX, (float)mouseY };
    const int SEG = 24;
    GizmoAxis best = GizmoAxis::None; float bestD = PICK_PIXELS;
    auto scan = [&](GizmoAxis ax) {
        XMFLOAT2 prev{}; bool havePrev = false;
        for (int i = 0; i <= SEG; i++) {
            float t = (float)i / SEG * XM_2PI;
            float a = std::cos(t) * L, b = std::sin(t) * L;
            XMFLOAT3 w;
            if (ax == GizmoAxis::X)      w = { center.x, center.y + a, center.z + b };
            else if (ax == GizmoAxis::Y) w = { center.x + a, center.y, center.z + b };
            else                         w = { center.x + a, center.y + b, center.z };
            XMFLOAT2 s;
            if (EditorPick_WorldToScreen(w, view, proj, s)) {
                if (havePrev) { float d = DistToSeg(p, prev, s); if (d < bestD) { bestD = d; best = ax; } }
                prev = s; havePrev = true;
            } else havePrev = false;
        }
    };
    scan(GizmoAxis::X); scan(GizmoAxis::Y); scan(GizmoAxis::Z);
    return best;
}

XMFLOAT3 EditorGizmo_DragScale(const Ray& ray) {
    // Reuse the translate plane math: signed distance along axis -> per-axis factor.
    XMFLOAT3 hit;
    if (!EditorPick_RayPlane(ray, g_PlanePt, g_PlaneN, hit)) return { 1, 1, 1 };
    XMVECTOR delta = XMLoadFloat3(&hit) - XMLoadFloat3(&g_DragStartHit);
    XMVECTOR ax = XMLoadFloat3(&g_AxisDir);
    float along = XMVectorGetX(XMVector3Dot(delta, ax));
    float f = 1.0f + along * 0.25f;
    if (f < 0.05f) f = 0.05f;
    XMFLOAT3 m = { 1, 1, 1 };
    if (g_Axis == GizmoAxis::X) m.x = f;
    else if (g_Axis == GizmoAxis::Y) m.y = f;
    else if (g_Axis == GizmoAxis::Z) m.z = f;
    else { m = { f, f, f }; }   // Uniform
    return m;
}

float EditorGizmo_DragRotate(const Ray& ray, const XMFLOAT3& center) {
    // Angle around the active axis, measured on the plane through center normal=axis.
    XMVECTOR ax = XMLoadFloat3(&g_AxisDir);
    XMFLOAT3 axisN; XMStoreFloat3(&axisN, ax);
    XMFLOAT3 hit;
    if (!EditorPick_RayPlane(ray, center, axisN, hit)) return 0.0f;
    XMVECTOR c = XMLoadFloat3(&center);
    XMVECTOR cur = XMVector3Normalize(XMLoadFloat3(&hit) - c);
    XMVECTOR start = XMVector3Normalize(XMLoadFloat3(&g_DragStartHit) - c);
    float dot = XMVectorGetX(XMVector3Dot(start, cur));
    dot = (dot < -1) ? -1 : (dot > 1 ? 1 : dot);
    float ang = std::acos(dot);
    // sign from the axis-oriented cross product
    XMVECTOR crs = XMVector3Cross(start, cur);
    if (XMVectorGetX(XMVector3Dot(crs, ax)) < 0) ang = -ang;
    return ang;
}

void EditorGizmo_BeginRingDrag(GizmoAxis axis, const XMFLOAT3& center, const Ray& ray) {
    g_Axis = axis;
    XMStoreFloat3(&g_AxisDir, AxisVec(axis));
    g_PlanePt = center; g_PlaneN = g_AxisDir;
    XMFLOAT3 hit;
    if (EditorPick_RayPlane(ray, center, g_PlaneN, hit)) g_DragStartHit = hit;
    else g_DragStartHit = center;
}
