// Billboard pixel shader — texture x vertex color x global tint.

struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

// Matches g_pPSConstantBuffer0 (Shader_Billboard_SetColor)
cbuffer PSColorBuffer : register(b0)
{
    float4 g_Color;
};

Texture2D tex : register(t0);
SamplerState samplerState : register(s0);

float4 main(PS_IN ps_in) : SV_TARGET
{
    float4 texColor = tex.Sample(samplerState, ps_in.uv);
    return texColor * ps_in.color * g_Color;
}
