// Constants shared with C++ — see Shaders/lighting_defines.h.
// The path is relative to the project root so fxc resolves it.
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
    float4 blend : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV
};

Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState samp;

float4 main(PS_IN pi):SV_TARGET
{
	//UV (45-degree rotation kept for the field material's checker look)
    float2 uv;
    float angle = 3.1415926535f * 45 / 180.0f;
    uv.x = pi.uv.x * cos(angle) + pi.uv.y * sin(angle);
    uv.y = -pi.uv.x * sin(angle) + pi.uv.y * cos(angle);

    //2つのテクスチャをブレンド
    float4 tex_color
		= tex0.Sample(samp, pi.uv) * pi.blend.g
		+ tex1.Sample(samp, pi.uv) * pi.blend.r;

	//材質の色
    float3 material_color = tex_color.rgb * diffuse_color.rgb;

    // Base lighting from the shared pipeline (hard-Lambert directional +
    // ambient + Phong spec). The historical half-Lambert wrap is layered on
    // below as a soft-lighting tweak so we don't fork the shared pipeline
    // just for this one legacy term.
    float3 color = ApplyLighting(material_color, pi.normalW.xyz, pi.posW.xyz);

    // Half-Lambert wrap: replace the hard-Lambert diffuse with a remapped
    // soft version (dot -1..1 -> 0..1). This is the historical behaviour on
    // terrain, kept intentionally.
    {
        float3 normalW = normalize(pi.normalW).xyz;
        float3 dir     = directional_world_vector.xyz;
        float  hardDl  = max(0.0f, dot(-dir, normalW));
        float  softDl  = (dot(-dir, normalW) + 1.0f) * 0.5f;
        color += material_color * directional_color.rgb * (softDl - hardDl);
    }

    // lighting_common.hlsli: accumulate point lights
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

    // Field is opaque regardless of texture alpha — it is a floor/terrain.
    return float4(color, 1.0f);

}