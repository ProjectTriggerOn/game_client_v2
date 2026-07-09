#pragma once
//=============================================================================
// editor_gizmo.h — translate/rotate/scale manipulators (spec §3.4, §8.2).
// Screen-space axis picking + plane-projected dragging. Draws with debug lines.
//=============================================================================
#include <DirectXMath.h>
#include "collision.h"   // Ray

enum class GizmoAxis { None, X, Y, Z, Uniform };

// Handle length in world units, scaled so it stays ~constant on screen.
float EditorGizmo_HandleLen(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye);

// Draw the translate manipulator (3 colored axes). `hot` is highlighted white.
void EditorGizmo_DrawTranslate(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye,
                               GizmoAxis hot, const DirectX::XMMATRIX& viewProj);

// Draw the scale manipulator (3 axes + center cube).
void EditorGizmo_DrawScale(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye,
                           GizmoAxis hot, const DirectX::XMMATRIX& viewProj);

// Draw the rotate manipulator (3 rings).
void EditorGizmo_DrawRotate(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye,
                            GizmoAxis hot, const DirectX::XMMATRIX& viewProj);

// Which linear axis (X/Y/Z) is under the cursor for translate/scale; None if none.
GizmoAxis EditorGizmo_PickAxis(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye,
                               int mouseX, int mouseY,
                               const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

// Which rotate ring (X/Y/Z) is under the cursor; None if none.
GizmoAxis EditorGizmo_PickRing(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& eye,
                               int mouseX, int mouseY,
                               const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj);

// Begin a drag on `axis`. Records the drag plane + start hit for the accumulators.
void EditorGizmo_BeginDrag(GizmoAxis axis, const DirectX::XMFLOAT3& center,
                           const Ray& ray, const DirectX::XMFLOAT3& eye);

// Begin a rotate drag: the drag plane is the ring plane (normal = axis through center).
void EditorGizmo_BeginRingDrag(GizmoAxis axis, const DirectX::XMFLOAT3& center, const Ray& ray);

// Accumulated results since BeginDrag:
DirectX::XMFLOAT3 EditorGizmo_DragTranslate(const Ray& ray);                 // world offset along axis
DirectX::XMFLOAT3 EditorGizmo_DragScale(const Ray& ray);                     // per-axis multiplier (>=0.05)
float             EditorGizmo_DragRotate(const Ray& ray, const DirectX::XMFLOAT3& center); // radians about axis
