
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

// Bone matrix constant buffer
cbuffer VS_BONE_BUFFER : register(b3)
{
    float4x4 boneTransforms[256];
}

struct VS_IN
{
    float4 posL : POSITION0;   // local position
    float3 normalL : NORMAL0;  // local normal
    float4 color : COLOR0;
    float2 uv : TEXCOORD0;
    float4 weights : WEIGHTS;  // bone weights
    uint4 bones : BONES;       // bone indices
};

struct VS_OUT
{
    float4 posH : SV_POSITION; // 変換後の座標
    float4 posW : POSITION0;   // ワールド座標
    float4 normalW : NORMAL0;  // ワールド法線
    float4 color : COLOR0;     // 色
    float2 uv : TEXCOORD0;     // uv
};

//=============================================================================
// 頂点シェ一ダ
//=============================================================================
VS_OUT main(VS_IN vi)
{
    VS_OUT vo;
    
    // Blend the four influencing bone transforms by their weights.
    float4x4 boneTransform =
        boneTransforms[vi.bones.x] * vi.weights.x +
        boneTransforms[vi.bones.y] * vi.weights.y +
        boneTransforms[vi.bones.z] * vi.weights.z +
        boneTransforms[vi.bones.w] * vi.weights.w;

    // Skin position and normal. Assumes bone transforms contain only rotation +
    // translation (no non-uniform scale), so the bone matrix can be applied to
    // the normal directly without an inverse-transpose.
    float4 posL_skinned = mul(vi.posL, boneTransform);
    float4 normalL_skinned = mul(float4(vi.normalL, 0.0f), boneTransform);

    float4x4 mtxWV = mul(world, view);
    float4x4 mtxWVP = mul(mtxWV, proj);
    
    vo.posH = mul(posL_skinned, mtxWVP);
    vo.posW = mul(posL_skinned, world);
    
    float4 normalW = mul(normalL_skinned, world);
    vo.normalW = normalize(normalW);
    
    vo.color = vi.color;
    vo.uv = vi.uv;

    return vo;
}
