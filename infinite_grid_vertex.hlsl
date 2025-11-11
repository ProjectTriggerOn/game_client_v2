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

float3 unproject_point(
    const in float x,
    const in float y,
    const in float z,
    const in float4x4 inv_view,
    const in float4x4 inv_proj)
{
    const float4x4 inv_view_inv_proj = mul(inv_proj, inv_view);
    const float4 unprojected_point = mul(float4(x, y, z, 1.0), inv_view_inv_proj);
    return unprojected_point.xyz / unprojected_point.w;
}

draw_infinite_grid__VS_output main(const in draw_infinite_grid__VS_input input)
{
    draw_infinite_grid__VS_output output = (draw_infinite_grid__VS_output)0;

    float3 p = g_grid_plane[input.vertex_id].xyz;
    output.near_point = unproject_point(p.x, p.y, 0.0, g_m_inv_view, g_m_inv_proj).xyz; // unprojecting on the near plane
    output.far_point = unproject_point(p.x, p.y, 1.0, g_m_inv_view, g_m_inv_proj).xyz; // unprojecting on the far plane
    output.vertex_position_inWVP = float4(p, 1.0);

    return output;
}
