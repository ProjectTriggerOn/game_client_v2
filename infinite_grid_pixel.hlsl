
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

static const float x_axis_width = 10.0;
static const float z_axis_width = 10.0;
static const float near_plane = 0.1;
static const float far_plane = 100;
static const float plane_width_scale_0 = 0.1;
static const float plane_width_scale_1 = 0.1;
static const float plane_color_intensity = 0.75;

//--------------------------------------------------------------------------------------
// Textures
//--------------------------------------------------------------------------------------
Texture2D    gex_decal : register(t0);;

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

float4 grid(const in float3 fragPos3D, const in float scale)
{
    const float2 coord = fragPos3D.xz * scale;
    const float2 derivative = fwidth(coord);
    const float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
    const float line_ = min(grid.x, grid.y);
    const float minimumz = min(derivative.y, 1);
    const float minimumx = min(derivative.x, 1);
    float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(line_, 1.0));
    // z axis
    if (fragPos3D.x > -z_axis_width * minimumx && fragPos3D.x < z_axis_width * minimumx)
        color.z = 1.0;
    // x axis
    if (fragPos3D.z > -z_axis_width * minimumz && fragPos3D.z < z_axis_width * minimumz)
        color.x = 1.0;
    return color;
}

float computeDepth(const in float3 pos)
{
    const float4x4 view_proj = mul(g_mView, g_mProj);
    const float4 unprojected_point = mul(float4(pos.xyz, 1.0), view_proj);
    return (unprojected_point.z / unprojected_point.w);
}

float computeLinearDepth(const in float3 pos)
{
    const float4x4 view_proj = mul(g_mView, g_mProj);
    const float4 clip_space_pos = mul(float4(pos.xyz, 1.0), view_proj);

    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0;
    float linearDepth = (2.0 * near_plane * far_plane) / (far_plane + near_plane - clip_space_depth * (far_plane - near_plane));
    return linearDepth / far_plane;
}

float4 main(draw_infinite_grid__VS_output input, out float depth : SV_Depth) : SV_Target
{
    const float t = -input.near_point.y / (input.far_point.y - input.near_point.y);
    const float3 fragPos3D = input.near_point + t * (input.far_point - input.near_point);
    const float4 tex_decal = gex_decal.Sample(g_sam_linear, fragPos3D.xz * float2(plane_width_scale_0, -plane_width_scale_0));
    depth = computeDepth(fragPos3D);
    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));

    float4 outColor;
    outColor = (tex_decal * plane_color_intensity + grid(fragPos3D, plane_width_scale_1)) * float(t > 0);
    outColor.a *= fading;
    return outColor;
}
