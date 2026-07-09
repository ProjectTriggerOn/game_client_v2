#pragma once
//=============================================================================
// editor_camera.h — Maya-style orbit camera for SCENE_EDITOR (spec §8.1).
// State: pivot P, distance d, yaw, pitch. eye = P + dir(yaw,pitch)*d, looks at P.
// Pure camera ops; the editor controller reads input and calls these.
//=============================================================================
#include <DirectXMath.h>

void EditorCamera_Init(const DirectX::XMFLOAT3& pivot, float dist, float yaw, float pitch);

void EditorCamera_Tumble(float dx, float dy);   // Alt+LMB: orbit P
void EditorCamera_Track(float dx, float dy);    // Alt+MMB: pan P (screen-constant)
void EditorCamera_Dolly(float steps);           // Alt+RMB / wheel: change d (+ = closer)

// Reframe so the AABB [mn,mx] fits the view (F frame-selected / A frame-all).
void EditorCamera_FrameBounds(const DirectX::XMFLOAT3& mn, const DirectX::XMFLOAT3& mx);

const DirectX::XMFLOAT4X4& EditorCamera_GetView();
const DirectX::XMFLOAT4X4& EditorCamera_GetProj();
const DirectX::XMFLOAT3&   EditorCamera_GetEye();
