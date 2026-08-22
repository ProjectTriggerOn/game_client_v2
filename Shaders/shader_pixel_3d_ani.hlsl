// Constants shared with C++ — see Shaders/lighting_defines.h.
#include "lighting_defines.h"

cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
}

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
}

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

cbuffer PS_CONSTANT_BUFFER : register(b3)
{
    float3 eye_pos;
    float specular_power = 30.0f;
    float4 specular_color = { 0.1f, 0.1f, 0.1f, 1.0f };
    float3 fog_color;
    float2 fog_params;   // x = start, y = end; end <= start disables fog
}

struct PointLight
{
    float3 posW;
    float range;
    float4 color;
};

cbuffer PS_CONSTANT_BUFFER : register(b4)
{
    PointLight point_light[LIGHT_MAX_POINT_LIGHTS];
    int point_light_count;
    float3 dummy;
}

// lighting_common.hlsli included AFTER cbuffers — fxc is single-pass and
// its functions reference these globals by name.
#include "lighting_common.hlsli"

struct PS_IN
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 posW : POSITION0;
    float4 normalW : NORMAL0; // 法線ワールド座標
    float4 color : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV
};

Texture2D tex;
SamplerState samp;

float4 main(PS_IN pi) : SV_TARGET
{
    // 材質の色
    float3 material_color = tex.Sample(samp, pi.uv).rgb * pi.color.rgb * diffuse_color.rgb;

    float3 color = ApplyLighting(material_color, pi.normalW.xyz, pi.posW.xyz);

    for (int i = 0; i < point_light_count; i++)
    {
        color += PointLightContribution(
            material_color,
            pi.normalW.xyz,
            pi.posW.xyz,
            point_light[i].posW,
            point_light[i].range,
            point_light[i].color);
    }

    // Distance fog — skip when the env has not authored a range.
    if (fog_params.y > fog_params.x)
    {
        color = ApplyFog(color, pi.posW.xyz);
    }

    float alpha = tex.Sample(samp, pi.uv).a * pi.color.a * diffuse_color.a;
    return float4(color, alpha);
}
