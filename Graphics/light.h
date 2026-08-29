#pragma once
#ifndef LIGHT_H
#define LIGHT_H

#include <d3d11.h>
#include <DirectXMath.h>

// Single source of truth for lighting constants shared C++ <-> HLSL.
// Include this first so LIGHT_MAX_POINT_LIGHTS and the LIGHT_CB_SLOT_* defines
// are visible to any code that reaches into raw shader constants.
#include "../Shaders/lighting_defines.h"

using namespace DirectX;

namespace triggeron
{
    // The canonical value lives in Shaders/lighting_defines.h. This using-alias
    // is kept so older call sites that wrote `triggeron::LIGHT_MAX_POINT_LIGHTS`
    // keep compiling; new code should reference the #define directly.
    inline constexpr int LIGHT_MAX_POINT_LIGHTS_CPP = LIGHT_MAX_POINT_LIGHTS;

    // cbuffer slot assignments, kept in one place so callers don't have to
    // know which b-register carries what. The HLSL side reads the same
    // integers from lighting_defines.h.
    enum class LightCbufferSlot : int
    {
        Material    = 0,   // per-shader own material color; not managed here
        Ambient     = LIGHT_CB_SLOT_AMBIENT,
        Directional = LIGHT_CB_SLOT_DIRECTIONAL,
        Specular    = LIGHT_CB_SLOT_SPECULAR,
        PointLights = LIGHT_CB_SLOT_POINTLIGHTS,
    };
}

// ---- GPU wire layouts -----------------------------------------------------
// Each struct mirrors one PS cbuffer exactly. Natural alignment + explicit
// static_asserts at the bottom of this file make the HLSL<->C++ contract a
// compile-time check instead of a runtime mystery.

struct PointLight
{
    DirectX::XMFLOAT3 LightPosition;   // float3
    float range;                       // float
    DirectX::XMFLOAT4 Color;           // float4
};

struct PointLightList
{
    PointLight pointLights[LIGHT_MAX_POINT_LIGHTS];
    int numPointLights;
    DirectX::XMFLOAT3 padding;
};

// 16-byte wide so Light_SetAmbient can upload an XMFLOAT4 and the shader's
// float4 ambient_color.w reads a defined value (1.0).
struct AmbientCB
{
    DirectX::XMFLOAT4 color;
};

// Direction is a pure vector: .w is always 0.0 by construction so the HLSL
// dot(-directional_world_vector, normalW) can't absorb a garbage w term.
struct DirectionalLight
{
    DirectX::XMFLOAT4 DirectionalW0;   // xyz = direction, w = 0.0
    DirectX::XMFLOAT4 Color;
};

struct SpecularLight
{
    DirectX::XMFLOAT3 CameraPosition;   // offset 0, 12 B
    float SpecularPower;                // offset 12, 4 B
    DirectX::XMFLOAT4 SpecularColor;    // offset 16, 16 B

    // Fog fields piggyback on the specular cbuffer (register b3) so we do
    // not grow the lighting state to a 5th cbuffer today. When fog, shadow
    // parameters, or wind get their own data they can move to b5.
    // Packed to avoid implicit padding:
    //   offset 32: FogColor float3 (12 B) + FogColorPad float (4 B) = 16 B
    //   offset 48: FogStart float (4 B) + FogEnd float (4 B) + pad (8 B) = 16 B
    DirectX::XMFLOAT3 FogColor;
    float FogColorPad;
    float FogStart;
    float FogEnd;
    DirectX::XMFLOAT2 _pad;             // tail padding to 64 B
};

// ---- CPU-side lighting environment ---------------------------------------
// Snapshot of everything one frame needs. Mutate via Light_SetEnvironment()
// (or the per-field helpers below), then the next Light_Flush() pushes exactly
// the cbuffers whose contents changed.

struct LightEnvironment
{
    DirectX::XMFLOAT4 ambient       = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT3 sunDirection  = { 0.0f, -1.0f, 0.0f };
    DirectX::XMFLOAT4 sunColor      = { 1.0f, 1.0f, 1.0f, 1.0f };
    float             specularPower = 30.0f;
    DirectX::XMFLOAT4 specularColor = { 0.1f, 0.1f, 0.1f, 1.0f };

    // Fog.  Applied as  final = mix(foggedColor, fogColor, fogFactor)  with
    // fogFactor = saturate((dist - fogStart) / (fogEnd - fogStart)); when
    // fogEnabled == 0 the shader skips the call entirely.
    DirectX::XMFLOAT3 fogColor   = { 0.0f, 0.0f, 0.0f };
    float             fogStart   = 0.0f;
    float             fogEnd     = 0.0f;
    int               fogEnabled = 0;      // non-zero => apply fog

    // Point lights remain under the legacy (mutable-index) API for now; the
    // env only carries the count so a future bulk-set has a place to park.
    int pointLightCount = 0;
};

// ---- Lifecycle ------------------------------------------------------------

// Creates the four PS cbuffer objects. Returns false if any creation fails;
// partial resources are released inside.
bool Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Light_Finalize(void);

// ---- Frame API ------------------------------------------------------------

// Snapshot a full environment. Marks changed fields dirty; does NOT upload.
void Light_SetEnvironment(const LightEnvironment& env);

// Read the current environment (for UIs, editors, save/load).
LightEnvironment Light_GetEnvironment();

// Convenience helpers for callers that only want to change one term. All mark
// the corresponding cbuffer dirty and return without touching the GPU.
void Light_SetAmbient(const DirectX::XMFLOAT3& color);

// dir is treated as a pure direction vector; only xyz are consumed. The cbuffer
// .w is forced to 0.0 here so the shader-side dot() against a float4 is safe.
void Light_SetDirectionalWorld(
    const DirectX::XMFLOAT4& direction,
    const DirectX::XMFLOAT4& color);

void Light_SetSpecularWorld(
    const DirectX::XMFLOAT3& camera_position,
    float specular_power,
    const DirectX::XMFLOAT4& specular_color);

// Fog.  enabled == false disables fog regardless of color/range.  fogStart
// and fogEnd are distances from the camera in world units; swap their order
// and the shader's saturate(fogFactor) stays valid.
void Light_SetFog(bool enabled,
                  const DirectX::XMFLOAT3& color,
                  float fogStart,
                  float fogEnd);

// Point-light state remains array-based. These update the internal list and
// mark the point-light cbuffer dirty; the actual upload happens in Flush.
void Light_SetPointLightByList(const PointLightList& list);
void Light_SetPointLightCount(int count);
void Light_SetPointLightWorldByCount(
    int index,
    const DirectX::XMFLOAT3& position,
    float range,
    const DirectX::XMFLOAT3& color);

// Single upload point. Issues UpdateSubresource + PSSetConstantBuffers for
// every cbuffer whose contents changed since the last call. Call once per
// frame, before any draw of lit geometry.
void Light_Flush(void);

// Reset dirty flags without uploading (used by tests and by scene transitions).
void Light_ClearDirty(void);

// ---- Layout guards ----------------------------------------------------------

static_assert(sizeof(PointLight)     == 32, "PointLight must match the HLSL float3+float4 layout");
static_assert(sizeof(PointLightList) == sizeof(PointLight) * LIGHT_MAX_POINT_LIGHTS + 16, "PointLightList must match HLSL cbuffer layout (4-byte count + 12-byte pad)");
static_assert(sizeof(AmbientCB)      == 16, "AmbientCB must be exactly one float4");
static_assert(sizeof(DirectionalLight) == 32, "DirectionalLight must be two float4s");
static_assert(sizeof(SpecularLight)  == 64, "SpecularLight must pack to 64 B: float3+float+float4+float3+float+float+float+float2");

#endif // LIGHT_H
