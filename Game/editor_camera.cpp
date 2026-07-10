//=============================================================================
// editor_camera.cpp — orbit camera (spec §8.1).
//=============================================================================
#ifdef EDITOR_ENABLED
#include "editor_camera.h"
#include "direct3d.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace {
    XMFLOAT3 g_Pivot { 0.0f, 1.0f, 0.0f };
    float    g_Dist  = 20.0f;
    float    g_Yaw   = 0.0f;     // radians, around +Y
    float    g_Pitch = 0.6f;     // radians, eye elevation above pivot

    XMFLOAT4X4 g_View {};
    XMFLOAT4X4 g_Proj {};
    XMFLOAT3   g_Eye  {};

    constexpr float ROT_SENS   = 0.008f;   // radians per pixel
    constexpr float TRACK_SENS = 0.0015f;  // pan units per pixel per unit distance
    constexpr float DOLLY_SENS = 0.1f;     // fraction of distance per wheel step
    constexpr float PITCH_LIMIT = 1.55f;   // ~89 degrees
    constexpr float DIST_MIN = 1.0f;
    constexpr float DIST_MAX = 500.0f;
    constexpr float FOV_DEG  = 60.0f;

    // Unit vector from pivot toward the eye.
    XMVECTOR OrbitDir() {
        float cp = std::cos(g_Pitch), sp = std::sin(g_Pitch);
        float cy = std::cos(g_Yaw),   sy = std::sin(g_Yaw);
        return XMVectorSet(cp * sy, sp, cp * cy, 0.0f);
    }

    void Recompute() {
        XMVECTOR pivot = XMLoadFloat3(&g_Pivot);
        XMVECTOR dir   = OrbitDir();
        XMVECTOR eye   = pivot + dir * g_Dist;
        XMStoreFloat3(&g_Eye, eye);

        XMMATRIX view = XMMatrixLookAtLH(eye, pivot, XMVectorSet(0, 1, 0, 0));
        XMStoreFloat4x4(&g_View, view);

        float aspect = static_cast<float>(Direct3D_GetBackBufferWidth()) /
                       static_cast<float>(Direct3D_GetBackBufferHeight());
        XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(FOV_DEG), aspect, 0.1f, 1000.0f);
        XMStoreFloat4x4(&g_Proj, proj);
    }
}

void EditorCamera_Init(const XMFLOAT3& pivot, float dist, float yaw, float pitch) {
    g_Pivot = pivot;
    g_Dist  = std::clamp(dist, DIST_MIN, DIST_MAX);
    g_Yaw   = yaw;
    g_Pitch = std::clamp(pitch, -PITCH_LIMIT, PITCH_LIMIT);
    Recompute();
}

void EditorCamera_Tumble(float dx, float dy) {
    g_Yaw   += dx * ROT_SENS;                 // flip sign here if it feels inverted
    g_Pitch += dy * ROT_SENS;
    g_Pitch  = std::clamp(g_Pitch, -PITCH_LIMIT, PITCH_LIMIT);
    Recompute();
}

void EditorCamera_Track(float dx, float dy) {
    XMVECTOR dir   = OrbitDir();                       // pivot->eye
    XMVECTOR fwd   = -dir;                             // eye->pivot
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), fwd));
    XMVECTOR up    = XMVector3Cross(fwd, right);
    float    scale = g_Dist * TRACK_SENS;
    XMVECTOR pivot = XMLoadFloat3(&g_Pivot);
    pivot += right * (-dx * scale) + up * (dy * scale);
    XMStoreFloat3(&g_Pivot, pivot);
    Recompute();
}

void EditorCamera_Dolly(float steps) {
    g_Dist *= std::pow(1.0f + DOLLY_SENS, -steps);      // steps>0 => closer
    g_Dist  = std::clamp(g_Dist, DIST_MIN, DIST_MAX);
    Recompute();
}

void EditorCamera_FrameBounds(const XMFLOAT3& mn, const XMFLOAT3& mx) {
    g_Pivot = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, (mn.z + mx.z) * 0.5f };
    float ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
    float radius = 0.5f * std::sqrt(ex * ex + ey * ey + ez * ez);
    if (radius < 0.5f) radius = 0.5f;
    float halfFov = XMConvertToRadians(FOV_DEG) * 0.5f;
    g_Dist = std::clamp(radius / std::tan(halfFov) * 1.3f, DIST_MIN, DIST_MAX);
    Recompute();
}

const XMFLOAT4X4& EditorCamera_GetView() { return g_View; }
const XMFLOAT4X4& EditorCamera_GetProj() { return g_Proj; }
const XMFLOAT3&   EditorCamera_GetEye()  { return g_Eye; }

#endif // EDITOR_ENABLED
