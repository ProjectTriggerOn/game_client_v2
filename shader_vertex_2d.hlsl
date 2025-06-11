
// 定数バッファ
float4x4 mtx;
struct VS_IN
{
    float4 posL : POSITION0; // ローカル座標
    float4 color : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV座標（必要に応じて追加）
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 color : COLOR0; // 色
    float2 uv : TEXCOORD0; // UV座標（必要に応じて追加）
};

//=============================================================================
// 頂点シェ一ダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    vo.posH  = mul(vi.posL, mtx); // ローカル座標を変換
    vo.color = vi.color; // 色をそのまま渡す
    vo.uv    = vi.uv; // UV座標をそのまま渡す（必要に応じて追加）
    return vo;
}
