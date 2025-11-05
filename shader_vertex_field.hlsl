
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

struct VS_IN
{
    float4 posL : POSITION0; // ローカル座標
    float4 normalL : NORMAL0; // 法線
    float4 blend : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 posW : POSITION0; // ワールド座標
    float4 normalW : NORMAL0; // ワールド法線
    float4 blend : COLOR0; // 色
    float2 uv : TEXCOORD0; // uv
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

    float4 normalW = mul(float4(vi.normalL.xyz, 0.0f), world);
    vo.normalW = normalize(normalW);
    vo.posW = mul(vi.posL, world);

    vo.blend = vi.blend; //地面のテクスチャのブレンド値はそのまま渡す

    vo.uv = vi.uv;

    return vo;
}
