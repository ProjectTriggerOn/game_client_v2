
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

    // ローカル座標を ワ一ルド → ビュー → プロジェクション 変換
    float4 posW = mul(vi.posL, view);
    float4 posWV = mul(posW, world);
    vo.posH = mul(posWV, proj); // ローカル座標を変換

    //float4x4 mtxV = mul(view, world);
    //float4x4 mtxWVP = mul(mtxV, proj);
    //vo.posH = mul(vi.posL, mtxWVP);



    vo.color = vi.color; // 色をそのまま渡す

    return vo;
}
