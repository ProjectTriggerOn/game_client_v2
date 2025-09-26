
struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
};

Texture2D tex0 : register(t0);

Texture2D tex1 : register(t1);
SamplerState samplerState;

float4 main(PS_IN ps_in) : SV_TARGET
{

    float uv = ps_in.uv * 0.1f;
    //uv.x = cos(3.1415926535f / 180 * 45) * ps_in.uv.x;
    //uv.y = sin(3.1415926535f / 180 * 45) * ps_in.uv.y;
    return
	tex0.Sample(samplerState, ps_in.uv) * 0.5f + tex0.Sample(samplerState, uv) * 0.5f
    //ps_in.color
	; // テクスチャサンプリング

}