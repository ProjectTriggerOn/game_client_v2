
struct PS_IN
{
    float4 posH : SV_POSITION;
    float4 color : COLOR0;
    float4 directional : COLOR1; // 頂点ライティングカラー
    float4 ambient : COLOR2; // 環境光カラー
    float2 uv : TEXCOORD0;
};

Texture2D tex0 : register(t0);

Texture2D tex1 : register(t1);
SamplerState samplerState;

//float4 main(PS_IN ps_in) : SV_TARGET


float4 main(PS_IN pi):SV_TARGET{
float2 uv;
float angle = 3.1415926535f * 45 / 180.0f;
uv.x = pi.uv.x * cos(angle) + pi.uv.y * sin(angle);
uv.y =-pi.uv.x * sin(angle) + pi.uv.y * cos(angle);

	float4 tex_color  = tex0.Sample(samplerState, pi.uv) * pi.color.g+ tex1.Sample(samplerState, pi.uv) * pi.color.r;
    return tex_color * pi.directional + tex_color * pi.ambient;
    //return tex0.Sample(samplerState, uv) * 0.5f + tex1.Sample(samplerState, pi.uv) * 0.5f;
}