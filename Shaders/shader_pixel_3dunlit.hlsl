cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 diffuse_color;
}

struct PS_IN
{
    float4 posH : SV_POSITION;
    float2 uv : TEXCOORD0; // UV座標（必要に応じて追加）
};

Texture2D tex;
SamplerState samplerState;

float4 main(PS_IN ps_in) : SV_TARGET
{
    float4 color = tex.Sample(samplerState, ps_in.uv) * diffuse_color; // テクスチャサンプリング

    // Alpha cutout: discard near-transparent fragments so they never write depth.
    // Blending alone is not enough here — Game_Draw() runs Map_Draw() *after* the
    // player draws, so a transparent fragment that wrote depth makes the map behind
    // it fail the LESS test, leaving the sky dome visible through the quad.
    // Safe for the sky dome (the only other ModelDrawUnlit caller): its texture is a
    // JPG with alpha 1.0 everywhere and Shader_3DUnlit_SetColor always passes a=1.0.
    clip(color.a - 0.5f);

    return color;
}