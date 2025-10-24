
// 定数バッファ
cbuffer VS_CONSTANT_BUFFER : register(b0)
{
    float4x4 world;

}

cbuffer VS_CONSTANT_BUFFER : register(b1)
{
    float4x4 view;
}

cbuffer VS_CONSTANT_BUFFER : register(b2)
{
    float4x4 proj;
}
cbuffer VS_CONSTANT_BUFFER : register(b3)
{
    float4 ambient_color;
}

cbuffer VS_CONSTANT_BUFFER : register(b4)
{
    float4 directional_world_vector;
    float4 directional_color;
}

struct VS_IN
{
    float4 posL : POSITION0; // ローカル座標
    float4 normalL : NORMAL0; // 法線
    float4 color : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 color : COLOR0; // 色
    float4 directional : COLOR1; // ライトカラー
    float4 ambient : COLOR2; // 環境光カラー
    float2 uv : TEXCOORD0; // UV
};

//=============================================================================
// 頂点シェ一ダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    // 座標変換
    float4 pos = float4(vi.posL.xyz, 1.0f); //补w
    pos = mul(pos, world); // 依次 world -> view -> proj
    pos = mul(pos, view);
    vo.posH = mul(pos, proj);

    // ライティング
    //普通のワールド変換行列を使うと拡大縮小の影響を受けてしまう
    // そこで逆行列の転置行列を使う
    float normalW = mul(float4(vi.normalL.xyz, 0.0f), world);
    normalW = normalize(normalW);
    float d1 = max(0.0f, dot(-directional_world_vector, vi.normalL));

    vo.color = vi.color; //地面のテクスチャのブレンド値はそのまま渡す

    vo.directional = float4(d1 * directional_color.rgb, 1.0f);
    vo.ambient = float4(ambient_color.rgb, 1.0f);

    //float3 color = d1 * directional_color.rgb + ambient_color.rgb;
    //vo.light_color = float4(color, 1.0f);

    vo.uv = vi.uv;
    return vo;
}
