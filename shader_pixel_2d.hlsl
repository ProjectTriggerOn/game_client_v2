struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0; // UV座標（必要に応じて追加）
};

Texture2D tex;
SamplerState samplerState;


float4 main(PS_IN ps_in) : SV_TARGET
{
    return tex.Sample(samplerState, ps_in.uv)*ps_in.color; // テクスチャサンプリング
}
