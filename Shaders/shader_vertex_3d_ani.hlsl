
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

// 骨骼矩阵常量缓冲区
cbuffer VS_BONE_BUFFER : register(b3)
{
    float4x4 boneTransforms[256];
}

struct VS_IN
{
    float4 posL : POSITION0;   // ローカル座標
    float3 normalL : NORMAL0;  // ローカル法線
    float4 color : COLOR0;     // 色
    float2 uv : TEXCOORD0;     // uv
    float4 weights : WEIGHTS;  // 骨骼权重
    uint4 bones : BONES;       // 骨骼索引
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
    
    // 骨骼变换计算
    float4x4 boneTransform = 
        boneTransforms[vi.bones.x] * vi.weights.x +
        boneTransforms[vi.bones.y] * vi.weights.y +
        boneTransforms[vi.bones.z] * vi.weights.z +
        boneTransforms[vi.bones.w] * vi.weights.w;

    // 应用骨骼变换到局部坐标
    float4 posL_skinned = mul(vi.posL, boneTransform);
    // 应用骨骼变换到局部法线 (注意：法线只需要旋转，不需要平移，但这里简化处理，假设矩阵包含旋转和平移)
    // 正确的做法是使用逆转置矩阵，但如果只有旋转和平移且没有非均匀缩放，直接使用左上3x3即可
    float4 normalL_skinned = mul(float4(vi.normalL, 0.0f), boneTransform);

    // 标准变换流程
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
