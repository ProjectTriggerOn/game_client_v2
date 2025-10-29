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
}

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

//float4 main(PS_IN ps_in) : SV_TARGET


float4 main(PS_IN pi):SV_TARGET
{
	//UV
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

    //並行光源
    float4 normalW = normalize(pi.normalW);
    //float dl = max(0.0f, dot(-directional_world_vector, normalW));
    float dl = (dot(-directional_world_vector, normalW) + 1.0f) * 0.5f;
    float3 diffuse = material_color * directional_color.rgb * dl;

    //環境光
    float3 ambient = material_color * ambient_color.rgb;

    //スペキュラ
    float3 toEye = normalize(eye_pos - pi.posW.xyz); //視点方向ベクトル(視点座標 - 頂点座標)
    float3 r = reflect(normalize(directional_world_vector), normalW).xyz; //反射ベクトル
    float t = pow(max(dot(r, toEye), 0.0f), specular_power); //スペキュラ強度
    float3 specular = specular_color * t;

    //float alpha = tex.Sample(samp, pi.uv).a * diffuse_color.a;
    float3 color = ambient + diffuse + specular; //最終的な目に届く色
    return float4(color, 1.0f);

}