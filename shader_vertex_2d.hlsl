
// 定数バッファ
float4x4 mtx;
struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 color : COLOR0; // 色
};

//=============================================================================
// 頂点シェ一ダ
//=============================================================================
VS_OUT main(in float4 posL : POSITION0,in float4 color : COLOR0)
{
    VS_OUT vo;
    vo.posH = mul(posL, mtx); // ローカル座標を変換
    vo.color = color; // 色をそのまま渡す
    return vo;
}
