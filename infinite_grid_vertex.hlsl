//--------------------------------------------------------------------------------------
// Constant buffers
//--------------------------------------------------------------------------------------
cbuffer cb_view_transform : register(b0)
{
    float4x4    g_mView;
    float4x4    g_mProj;
    float4x4    g_m_inv_view;
    float4x4    g_m_inv_proj;
};

// 屏幕空间大三角形
static const float3 g_grid_plane[6] =
{
    float3(1, 1, 0),   float3(-1, -1, 0), float3(-1, 1, 0),
    float3(-1, -1, 0), float3(1, 1, 0),   float3(1, -1, 0)
};

struct draw_infinite_grid__VS_input
{
    float4    position : POSITION;
    uint      vertex_id : SV_VertexID;
};

struct draw_infinite_grid__VS_output
{
    float4    vertex_position_inWVP : SV_POSITION;
    float3    near_point : TEXCOORD0;
    float3    far_point : TEXCOORD1;
};

// 屏幕空间->世界空间反投影
float3 unproject_point(
    float x,
    float y,
    float z,
    float4x4 inv_view,
    float4x4 inv_proj)
{
    // 注意顺序：先投影逆，再视图逆
    float4x4 inv_view_inv_proj = mul(inv_view, inv_proj);
    float4 unprojected_point = mul(float4(x, y, z, 1.0), inv_view_inv_proj);
    return unprojected_point.xyz / unprojected_point.w;
}

draw_infinite_grid__VS_output main(draw_infinite_grid__VS_input input)
{
    draw_infinite_grid__VS_output output = (draw_infinite_grid__VS_output)0;

    float3 p = g_grid_plane[input.vertex_id].xyz;
    output.near_point = unproject_point(p.x, p.y, 0.0, g_m_inv_view, g_m_inv_proj); // near plane
    output.far_point = unproject_point(p.x, p.y, 1.0, g_m_inv_view, g_m_inv_proj); // far plane
    output.vertex_position_inWVP = float4(p, 1.0);

    return output;
}
