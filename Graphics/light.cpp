//=============================================================================
// light.cpp
//
// Lighting state owner. Keeps a CPU-side LightEnvironment, owns the four PS
// cbuffers, and flushes only what changed. HLSL contract (b1/b2/b3/b4 slots,
// cbuffer layouts) is pinned by the static_asserts in light.h.
//=============================================================================

#include "direct3d.h"
#include "debug_ostream.h"
#include "Light.h"

using namespace DirectX;

namespace
{
    ID3D11Buffer*        g_pPSConstantBufferAmbient     = nullptr;   // slot b1
    ID3D11Buffer*        g_pPSConstantBufferDirectional = nullptr;   // slot b2
    ID3D11Buffer*        g_pPSConstantBufferSpecular    = nullptr;   // slot b3
    ID3D11Buffer*        g_pPSConstantBufferPointLights = nullptr;   // slot b4
    ID3D11DeviceContext* g_pContext = nullptr;
    ID3D11Device*        g_pDevice  = nullptr;

    LightEnvironment g_environment{};
    PointLightList   g_pointLights{};

    // Specular packs eye position + power + color into one cbuffer. The eye
    // is supplied by the caller every frame, so this struct is the source of
    // truth for what gets uploaded; the env mirrors only power and color.
    SpecularLight g_specularPacked{};

    enum DirtyBit : UINT
    {
        DIRTY_AMBIENT     = 1 << 0,
        DIRTY_DIRECTIONAL = 1 << 1,
        DIRTY_SPECULAR    = 1 << 2,
        DIRTY_POINTLIGHT  = 1 << 3,
        DIRTY_ALL         = DIRTY_AMBIENT | DIRTY_DIRECTIONAL | DIRTY_SPECULAR | DIRTY_POINTLIGHT,
    };

    UINT g_dirty = DIRTY_ALL;

    UINT SlotOf(DirtyBit bit)
    {
        switch (bit)
        {
        case DIRTY_AMBIENT:     return static_cast<UINT>(triggeron::LightCbufferSlot::Ambient);
        case DIRTY_DIRECTIONAL: return static_cast<UINT>(triggeron::LightCbufferSlot::Directional);
        case DIRTY_SPECULAR:    return static_cast<UINT>(triggeron::LightCbufferSlot::Specular);
        case DIRTY_POINTLIGHT:  return static_cast<UINT>(triggeron::LightCbufferSlot::PointLights);
        default:                return static_cast<UINT>(triggeron::LightCbufferSlot::Material);
        }
    }

    void UploadIfDirty(DirtyBit bit, ID3D11Buffer* buf, const void* data)
    {
        if (!(g_dirty & static_cast<UINT>(bit))) return;
        g_pContext->UpdateSubresource(buf, 0, nullptr, data, 0, 0);
        const UINT slot = SlotOf(bit);
        g_pContext->PSSetConstantBuffers(slot, 1, &buf);
        g_dirty &= ~static_cast<UINT>(bit);
    }
}

bool Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
    if (!pDevice || !pContext) {
        hal::dout << "Light_Initialize() : the given device or device context is invalid" << std::endl;
        return false;
    }

    g_pDevice  = pDevice;
    g_pContext = pContext;

    D3D11_BUFFER_DESC buffer_desc{};
    buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    struct MakeSlot { ID3D11Buffer** out; size_t bytes; const char* name; };
    const MakeSlot slots[] = {
        { &g_pPSConstantBufferAmbient,     sizeof(AmbientCB),        "ambient"      },
        { &g_pPSConstantBufferDirectional, sizeof(DirectionalLight), "directional"  },
        { &g_pPSConstantBufferSpecular,    sizeof(SpecularLight),    "specular"     },
        { &g_pPSConstantBufferPointLights, sizeof(PointLightList),   "point lights" },
    };

    for (const MakeSlot& s : slots)
    {
        buffer_desc.ByteWidth = static_cast<UINT>(s.bytes);
        const HRESULT hr = g_pDevice->CreateBuffer(&buffer_desc, nullptr, s.out);
        if (FAILED(hr)) {
            hal::dout << "Light_Initialize() : failed to create " << s.name << " cbuffer" << std::endl;
            Light_Finalize();
            return false;
        }
    }

    // Seed the environment with the historical defaults so the first-frame
    // look matches what the previous implementation produced, then force a
    // full upload so nothing samples zero-filled memory before the first
    // Light_Flush().
    Light_SetEnvironment(LightEnvironment{});
    g_dirty = DIRTY_ALL;
    return true;
}

void Light_Finalize(void)
{
    SAFE_RELEASE(g_pPSConstantBufferPointLights)
    SAFE_RELEASE(g_pPSConstantBufferSpecular)
    SAFE_RELEASE(g_pPSConstantBufferDirectional)
    SAFE_RELEASE(g_pPSConstantBufferAmbient)
    g_pDevice  = nullptr;
    g_pContext = nullptr;
}

// ---- Environment setters ---------------------------------------------------

void Light_SetEnvironment(const LightEnvironment& env)
{
    if (env.ambient.x != g_environment.ambient.x
     || env.ambient.y != g_environment.ambient.y
     || env.ambient.z != g_environment.ambient.z
     || env.ambient.w != g_environment.ambient.w)
    {
        g_environment.ambient = env.ambient;
        g_dirty |= DIRTY_AMBIENT;
    }

    if (env.sunDirection.x != g_environment.sunDirection.x
     || env.sunDirection.y != g_environment.sunDirection.y
     || env.sunDirection.z != g_environment.sunDirection.z
     || env.sunColor.x     != g_environment.sunColor.x
     || env.sunColor.y     != g_environment.sunColor.y
     || env.sunColor.z     != g_environment.sunColor.z
     || env.sunColor.w     != g_environment.sunColor.w)
    {
        g_environment.sunDirection = env.sunDirection;
        g_environment.sunColor     = env.sunColor;
        g_dirty |= DIRTY_DIRECTIONAL;
    }

    if (env.specularPower   != g_environment.specularPower
     || env.specularColor.x != g_environment.specularColor.x
     || env.specularColor.y != g_environment.specularColor.y
     || env.specularColor.z != g_environment.specularColor.z
     || env.specularColor.w != g_environment.specularColor.w)
    {
        g_environment.specularPower = env.specularPower;
        g_environment.specularColor = env.specularColor;
        g_specularPacked.SpecularPower = env.specularPower;
        g_specularPacked.SpecularColor = env.specularColor;
        g_dirty |= DIRTY_SPECULAR;
    }

    if (env.fogEnabled != g_environment.fogEnabled
     || env.fogColor.x != g_environment.fogColor.x
     || env.fogColor.y != g_environment.fogColor.y
     || env.fogColor.z != g_environment.fogColor.z
     || env.fogStart   != g_environment.fogStart
     || env.fogEnd     != g_environment.fogEnd)
    {
        g_environment.fogEnabled = env.fogEnabled;
        g_environment.fogColor   = env.fogColor;
        g_environment.fogStart   = env.fogStart;
        g_environment.fogEnd     = env.fogEnd;
        g_specularPacked.FogColor = env.fogColor;
        g_specularPacked.FogStart = env.fogStart;
        g_specularPacked.FogEnd   = env.fogEnd;
        g_dirty |= DIRTY_SPECULAR;
    }

    if (env.pointLightCount != g_environment.pointLightCount)
    {
        g_environment.pointLightCount = env.pointLightCount;
        g_pointLights.numPointLights  = env.pointLightCount;
        g_dirty |= DIRTY_POINTLIGHT;
    }
}

LightEnvironment Light_GetEnvironment()
{
    return g_environment;
}

void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
    LightEnvironment env = g_environment;
    env.ambient = { color.x, color.y, color.z, 1.0f };
    Light_SetEnvironment(env);
}

void Light_SetDirectionalWorld(const DirectX::XMFLOAT4& direction, const DirectX::XMFLOAT4& color)
{
    // Only xyz of the input are meaningful; the cbuffer's .w is forced to 0
    // by the pack step in Flush so the shader's float4 dot can't absorb a
    // garbage w term.
    LightEnvironment env = g_environment;
    env.sunDirection = { direction.x, direction.y, direction.z };
    env.sunColor     = color;
    Light_SetEnvironment(env);
}

void Light_SetSpecularWorld(const DirectX::XMFLOAT3& camera_position,
                            float specular_power,
                            const DirectX::XMFLOAT4& specular_color)
{
    const bool powerColorChanged =
           g_specularPacked.SpecularPower   != specular_power
        || g_specularPacked.SpecularColor.x != specular_color.x
        || g_specularPacked.SpecularColor.y != specular_color.y
        || g_specularPacked.SpecularColor.z != specular_color.z
        || g_specularPacked.SpecularColor.w != specular_color.w;

    const bool eyeChanged =
           g_specularPacked.CameraPosition.x != camera_position.x
        || g_specularPacked.CameraPosition.y != camera_position.y
        || g_specularPacked.CameraPosition.z != camera_position.z;

    if (!powerColorChanged && !eyeChanged) return;

    g_specularPacked.CameraPosition = camera_position;
    g_specularPacked.SpecularPower  = specular_power;
    g_specularPacked.SpecularColor  = specular_color;

    // Mirror into the env so Light_GetEnvironment reports the latest values.
    g_environment.specularPower = specular_power;
    g_environment.specularColor = specular_color;

    g_dirty |= DIRTY_SPECULAR;
}

void Light_SetFog(bool enabled,
                  const DirectX::XMFLOAT3& color,
                  float fogStart,
                  float fogEnd)
{
    const int enabledInt = enabled ? 1 : 0;
    const bool changed =
           g_environment.fogEnabled != enabledInt
        || g_environment.fogColor.x != color.x
        || g_environment.fogColor.y != color.y
        || g_environment.fogColor.z != color.z
        || g_environment.fogStart   != fogStart
        || g_environment.fogEnd     != fogEnd;

    if (!changed) return;

    g_environment.fogEnabled = enabledInt;
    g_environment.fogColor   = color;
    g_environment.fogStart   = fogStart;
    g_environment.fogEnd     = fogEnd;

    // Fog shares the specular cbuffer — piggyback the new fields onto it.
    g_specularPacked.FogColor = color;
    g_specularPacked.FogStart = fogStart;
    g_specularPacked.FogEnd   = fogEnd;

    g_dirty |= DIRTY_SPECULAR;
}

// ---- Point-light state ----------------------------------------------------

void Light_SetPointLightByList(const PointLightList& list)
{
    g_pointLights = list;
    g_environment.pointLightCount = list.numPointLights;
    g_dirty |= DIRTY_POINTLIGHT;
}

void Light_SetPointLightCount(int count)
{
    if (count < 0) count = 0;
    if (count > LIGHT_MAX_POINT_LIGHTS) count = LIGHT_MAX_POINT_LIGHTS;
    if (count == g_environment.pointLightCount) return;

    g_pointLights.numPointLights  = count;
    g_environment.pointLightCount = count;
    g_dirty |= DIRTY_POINTLIGHT;
}

void Light_SetPointLightWorldByCount(int index,
                                    const XMFLOAT3& position,
                                    float range,
                                    const XMFLOAT3& color)
{
    if (index < 0 || index >= LIGHT_MAX_POINT_LIGHTS) return;
    if (index >= g_pointLights.numPointLights) return;

    PointLight& dst = g_pointLights.pointLights[index];
    if (dst.LightPosition.x == position.x
     && dst.LightPosition.y == position.y
     && dst.LightPosition.z == position.z
     && dst.range           == range
     && dst.Color.x         == color.x
     && dst.Color.y         == color.y
     && dst.Color.z         == color.z
     && dst.Color.w         == 1.0f)
    {
        return;
    }

    dst.LightPosition = position;
    dst.range         = range;
    dst.Color         = { color.x, color.y, color.z, 1.0f };
    g_dirty |= DIRTY_POINTLIGHT;
}

// ---- Flush ------------------------------------------------------------------

void Light_Flush(void)
{
    if (!g_pContext) return;

    const AmbientCB ambient{ g_environment.ambient };
    UploadIfDirty(DIRTY_AMBIENT, g_pPSConstantBufferAmbient, &ambient);

    const DirectionalLight dir{
        { g_environment.sunDirection.x, g_environment.sunDirection.y, g_environment.sunDirection.z, 0.0f },
        g_environment.sunColor
    };
    UploadIfDirty(DIRTY_DIRECTIONAL, g_pPSConstantBufferDirectional, &dir);

    UploadIfDirty(DIRTY_SPECULAR,   g_pPSConstantBufferSpecular,   &g_specularPacked);
    UploadIfDirty(DIRTY_POINTLIGHT,g_pPSConstantBufferPointLights,&g_pointLights);
}

void Light_ClearDirty(void)
{
    g_dirty = 0;
}
