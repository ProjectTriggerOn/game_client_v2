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
    float2 uv : TEXCOORD0; // uv
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float2 uv : TEXCOORD0; // uv
};

VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    vo.posH = mul(vi.posL, mtxWVP);
    vo.uv = vi.uv;

    return vo;
}