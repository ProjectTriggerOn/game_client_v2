//=============================================================================
// lighting_common.hlsli
//
// Shared lighting pipeline used by the three lit pixel shaders
// (shader_pixel_3d.hlsl, shader_pixel_3d_ani.hlsl, shader_pixel_field.hlsl).
//
// Caller owns all cbuffers, samplers, and textures — we provide only the
// lighting math. Functions below reference the caller's cbuffer globals
// directly (HLSL allows this: compile order within a translation unit is
// not required for globals referenced from function bodies).
//
// Required globals in the calling shader, checked by name at compile time:
//   cbuffer b1: float4 ambient_color
//   cbuffer b2: float4 directional_world_vector  (world-space, w = 0)
//               float4 directional_color
//   cbuffer b3: float3 eye_pos
//               float  specular_power
//               float4 specular_color
//               float3 fog_color
//               float2 fog_params       // x = start, y = end (distances)
//
// fog_params.y <= fog_params.x (or == 0) means "fog disabled" — the caller
// gates ApplyFog() on that condition.
//
//
// The PointLight struct itself is caller-declared (legacy; see the three
// shader files). See Shaders/lighting_defines.h for the constant that pins
// LIGHT_MAX_POINT_LIGHTS on both sides of the C++/HLSL boundary.
//=============================================================================

#ifndef TRIGGERON_LIGHTING_COMMON_HLSLI
#define TRIGGERON_LIGHTING_COMMON_HLSLI

#include "lighting_defines.h"

//-----------------------------------------------------------------------------
// Direct lighting: directional + ambient + Phong spec (no point lights).
// material_color is the fully-combined albedo (tex * vertexColor * diffuse).
// Returns the lit color before any point-light contribution is added.
//-----------------------------------------------------------------------------
float3 ApplyLighting(
    float3 material_color,
    float3 normalW,
    float3 posW)
{
    float3 normal = normalize(normalW);

    // Trim the directional vector's w (forced to 0 by the C++ side) to a
    // float3 so dot()/reflect() work in float3 space cleanly.
    float3 dir = directional_world_vector.xyz;

    // Parallel directional light (Lambert)
    float dl = max(0.0f, dot(-dir, normal));
    float3 diffuse = material_color * directional_color.rgb * dl;

    // Ambient
    float3 ambient = material_color * ambient_color.rgb;

    // Specular (Phong)
    float3 toEye = normalize(eye_pos - posW);
    float3 r = reflect(normalize(dir), normal);
    float t = pow(max(dot(r, toEye), 0.0f), specular_power);
    float3 specular = specular_color.rgb * t;

    return ambient + diffuse + specular;
}

//-----------------------------------------------------------------------------
// One point-light contribution (diffuse + specular). Accumulate over the
// visible lights in a loop. Caller owns the PointLight struct / cbuffer b4.
//
// Attenuation:  A = (max(1 - dist/range, 0))^2  — range<=0 treats the light
//               as disabled instead of dividing by zero.
// Diffuse:      material * color * A * max(0, dot(-normalize(LtoP), N))
// Specular:     color * pow(max(dot(reflect(normalize(LtoP), N), toEye), 0),
//                          specular_power)
//               NOTE: the historical point-light spec uses the light's own
//               color (not the specular_color cbuffer), so we do the same
//               here — changing the multiplier is a separate visual decision.
//-----------------------------------------------------------------------------
float3 PointLightContribution(
    float3 material_color,
    float3 normalW,
    float3 posW,
    float3 light_posW,
    float light_range,
    float4 light_color)
{
    float3 normal = normalize(normalW);

    float3 lightToPixel = posW - light_posW;
    float dist = length(lightToPixel);

    float A = 0.0f;
    if (light_range > 0.0f)
    {
        float oneMinus = max(1.0f - dist / light_range, 0.0f);
        A = oneMinus * oneMinus;
    }

    // Light direction toward the surface (from the light to the pixel).
    float3 LtoP = normalize(lightToPixel);
    float3 diffuse = material_color * light_color.rgb * A
                   * max(0.0f, dot(-LtoP, normal));

    float3 toEye = normalize(eye_pos - posW);
    float3 r     = reflect(LtoP, normal);
    float  t     = pow(max(dot(r, toEye), 0.0f), specular_power);
    float3 spec  = light_color.rgb * t;

    return diffuse + spec;
}

//-----------------------------------------------------------------------------
// Distance fog. factor = saturate((dist - start) / (end - start)). The
// caller decides whether to apply at all (fogEnd > fogStart gate keeps an
// all-zero env block "off by default" without needing a separate flag).
// Applied to the lit color BEFORE alpha — alpha is untouched.
//-----------------------------------------------------------------------------
float3 ApplyFog(float3 color, float3 posW)
{
    float dist = length(eye_pos - posW);
    float fogStart = fog_params.x;
    float fogEnd   = fog_params.y;
    float range    = max(fogEnd - fogStart, 1e-6f);
    float factor   = saturate((dist - fogStart) / range);
    // Linear interpolation between the lit color and the fog color. Fog is
    // applied last so the unlit sky / UI can skip it cleanly.
    return lerp(color, fog_color, factor);
}

#endif // TRIGGERON_LIGHTING_COMMON_HLSLI
