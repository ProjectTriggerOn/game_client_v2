//=============================================================================
// editor_pick.cpp — screen<->world math (spec §3.3, §8.2).
//=============================================================================
#include "editor_pick.h"
#include "direct3d.h"
#include <cmath>

using namespace DirectX;

Ray EditorPick_ScreenRay(int sx, int sy, const XMFLOAT4X4& view, const XMFLOAT4X4& proj) {
    float w = static_cast<float>(Direct3D_GetBackBufferWidth());
    float h = static_cast<float>(Direct3D_GetBackBufferHeight());
    float ndcX = (2.0f * sx / w) - 1.0f;
    float ndcY = 1.0f - (2.0f * sy / h);          // pixel origin top-left -> NDC +Y up

    XMMATRIX vp = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj);
    XMMATRIX inv = XMMatrixInverse(nullptr, vp);

    XMVECTOR pNear = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), inv);
    XMVECTOR pFar  = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), inv);
    XMVECTOR dir   = XMVector3Normalize(pFar - pNear);

    Ray r;
    XMStoreFloat3(&r.origin, pNear);
    XMStoreFloat3(&r.direction, dir);
    return r;
}

bool EditorPick_RayAABB(const Ray& r, const AABB& box, float& tHit) {
    float tmin = 0.0f, tmax = 1e30f;
    const float o[3] = { r.origin.x, r.origin.y, r.origin.z };
    const float d[3] = { r.direction.x, r.direction.y, r.direction.z };
    const float lo[3] = { box.min.x, box.min.y, box.min.z };
    const float hi[3] = { box.max.x, box.max.y, box.max.z };
    for (int i = 0; i < 3; i++) {
        if (std::fabs(d[i]) < 1e-8f) {
            if (o[i] < lo[i] || o[i] > hi[i]) return false;   // parallel & outside slab
        } else {
            float inv = 1.0f / d[i];
            float t1 = (lo[i] - o[i]) * inv;
            float t2 = (hi[i] - o[i]) * inv;
            if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return false;
        }
    }
    tHit = tmin;
    return true;
}

bool EditorPick_RayPlane(const Ray& r, const XMFLOAT3& planePt, const XMFLOAT3& planeN, XMFLOAT3& hit) {
    XMVECTOR n  = XMVector3Normalize(XMLoadFloat3(&planeN));
    XMVECTOR d  = XMLoadFloat3(&r.direction);
    float denom = XMVectorGetX(XMVector3Dot(n, d));
    if (std::fabs(denom) < 1e-8f) return false;              // parallel
    XMVECTOR o   = XMLoadFloat3(&r.origin);
    XMVECTOR p   = XMLoadFloat3(&planePt);
    float t = XMVectorGetX(XMVector3Dot(n, p - o)) / denom;
    if (t < 0.0f) return false;                             // behind the ray origin
    XMStoreFloat3(&hit, o + d * t);
    return true;
}

bool EditorPick_WorldToScreen(const XMFLOAT3& wpt, const XMFLOAT4X4& view, const XMFLOAT4X4& proj, XMFLOAT2& screen) {
    XMMATRIX vp = XMLoadFloat4x4(&view) * XMLoadFloat4x4(&proj);
    XMVECTOR clip = XMVector3Transform(XMVectorSetW(XMLoadFloat3(&wpt), 1.0f), vp);
    float wc = XMVectorGetW(clip);
    if (wc <= 1e-6f) return false;                          // behind camera
    float ndcX = XMVectorGetX(clip) / wc;
    float ndcY = XMVectorGetY(clip) / wc;
    float w = static_cast<float>(Direct3D_GetBackBufferWidth());
    float h = static_cast<float>(Direct3D_GetBackBufferHeight());
    screen.x = (ndcX * 0.5f + 0.5f) * w;
    screen.y = (1.0f - (ndcY * 0.5f + 0.5f)) * h;
    return true;
}

bool EditorPick_RayGroundY(const Ray& r, float planeY, XMFLOAT3& hit) {
    return EditorPick_RayPlane(r, { 0, planeY, 0 }, { 0, 1, 0 }, hit);
}
