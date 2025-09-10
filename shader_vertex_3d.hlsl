
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
    float4 color : COLOR0; // 色
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 color : COLOR0; // 色
};

//=============================================================================
// 頂点シェ一ダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;

    float4 pos = float4(vi.posL.xyz, 1.0f); //补w
    pos = mul(pos, world); // 依次 world -> view -> proj
    pos = mul(pos, view);
    vo.posH = mul(pos, proj);
    vo.color = vi.color;
    return vo;
}
