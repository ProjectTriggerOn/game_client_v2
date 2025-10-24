cbuffer PS_CONSTANT_BUFFER : register(b0)
{
    float4 color;

}

cbuffer PS_CONSTANT_BUFFER : register(b1)
{
    float4 ambient_color;
}

cbuffer PS_CONSTANT_BUFFER : register(b2)
{
    float4 directional_world_vector;
    float4 directional_color;
    float3 eye_pos;
}

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
    //並行光源
    float4 normalW = normalize(pi.normalW);
    float dl = max(0.0f, dot(-directional_world_vector, normalW));

    float3 toEye = normalize(eye_pos - pi.posW.xyz); //視点方向ベクトル(視点座標 - 頂点座標)
    float3 r = reflect(normalize(directional_world_vector), normalW).xyz; //反射ベクトル
    float t = pow(max(dot(r, toEye), 0.0f), 10.0f); //スペキュラ強度

    float3 l_color = pi.color.rgb * directional_color.rgb * dl
	+ ambient_color.rgb * pi.color.rgb;
    l_color += float3(1.0f, 1.0f, 1.0f) * t;

    return tex.Sample(samp, pi.uv) * float4(l_color, 1.0f) * color; // テクスチャサンプリング

}