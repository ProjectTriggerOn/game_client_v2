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

cbuffer cb_grid_params : register(b1)
{
    float4 grid_axis_widths;      // x: x_axis_width, y: z_axis_width, z: near_plane, w: far_plane
    float4 grid_plane_params;     // x: plane_width_scale_0, y: plane_width_scale_1, z: plane_color_intensity, w: padding
    float4 grid_fade_params;      // x: grid_fade_start, y: grid_fade_end, zw: padding
}

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D    gex_decal : register(t0);

//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
SamplerState g_sam_linear;

struct draw_infinite_grid__VS_output
{
    float4    vertex_position_inWVP : SV_POSITION;
    float3    near_point : TEXCOORD0;
    float3    far_point : TEXCOORD1;
};

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

// 抗锯齿格子线
float4 grid(const float3 fragPos3D, float scale, float x_axis_width, float z_axis_width)
{
    float2 coord = fragPos3D.xz * scale;
    float2 derivative = fwidth(coord);
    float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    float line_ = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(line_, 1.0));
    // z axis
    if (fragPos3D.x > -z_axis_width * minimumx && fragPos3D.x < z_axis_width * minimumx)
        color.z = 1.0;
    // x axis
    if (fragPos3D.z > -x_axis_width * minimumz && fragPos3D.z < x_axis_width * minimumz)
        color.x = 1.0;
    return color;
}

float computeDepth(const float3 pos, float4x4 g_mView, float4x4 g_mProj)
{
    float4 view_pos = mul(float4(pos, 1.0), g_mView);
    float4 clip_pos = mul(view_pos, g_mProj);
    return clip_pos.z / clip_pos.w;
}

float computeLinearDepth(const float3 pos, float4x4 g_mView, float4x4 g_mProj,
                         float near_plane, float far_plane)
{
    float4 view_pos = mul(float4(pos, 1.0), g_mView);
    float4 clip_pos = mul(view_pos, g_mProj);
    float clip_space_depth = clip_pos.z / clip_pos.w;
    float linearDepth = (2.0 * near_plane * far_plane) /
        (far_plane + near_plane - (2.0 * clip_space_depth - 1.0) * (far_plane - near_plane));
    return saturate(linearDepth / far_plane);
}


float4 main(draw_infinite_grid__VS_output input, out float depth : SV_Depth) : SV_Target
{
    // 解析参数
    float x_axis_width = grid_axis_widths.x;
    float z_axis_width = grid_axis_widths.y;
    float near_plane = grid_axis_widths.z;
    float far_plane = grid_axis_widths.w;

    float plane_width_scale_0 = grid_plane_params.x;
    float plane_width_scale_1 = grid_plane_params.y;
    float plane_color_intensity = grid_plane_params.z;

    float grid_fade_start = grid_fade_params.x;
    float grid_fade_end = grid_fade_params.y;

    // 计算地面交点
    float t = -input.near_point.y / (input.far_point.y - input.near_point.y);
    float3 fragPos3D = input.near_point + t * (input.far_point - input.near_point);

    // 采样贴图
    float2 uv = fragPos3D.xz * plane_width_scale_0;
    float4 tex_decal = gex_decal.Sample(g_sam_linear, uv);

    // 计算深度
    depth = computeDepth(fragPos3D, g_mView, g_mProj);
    float linearDepth = computeLinearDepth(fragPos3D, g_mView, g_mProj, near_plane, far_plane);

    // 格子淡出
    float fading = smoothstep(grid_fade_end, grid_fade_start, linearDepth);

    // 叠加贴图和格子线
    float4 outColor = (tex_decal * plane_color_intensity + grid(fragPos3D, plane_width_scale_1, x_axis_width, z_axis_width)) * float(t > 0);
    outColor.a *= fading;
    return outColor;
    //return float4(1, 0, 0, 1); // 全屏红色

}
