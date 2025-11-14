#include "infinite_grid.h"
#include "shader_infinite.h"
#include <d3d11.h>

#include "camera.h"

// 局部保存 device/context，约定只初始化一次
namespace
{
    bool g_isInited = false;
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
}

// =================== 初始化 ========================
bool InfiniteGrid_Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (!device || !context)
        return false;

    if (Shader_InfiniteGrid_Initialize(device, context))
    {
        g_device = device;
        g_context = context;
        g_isInited = true;
        return true;
    }
    return false;
}

// ==================== 销毁 ========================
void InfiniteGrid_Finalize()
{
    if (g_isInited)
    {
        Shader_InfiniteGrid_Finalize();
        g_isInited = false;
        g_device = nullptr;
        g_context = nullptr;
    }
}

// ===================== 绘制 ========================

void InfiniteGrid_Draw()
{
    if (!g_isInited) return;

    Shader_InfiniteGrid_Begin();

    GridParams params = {};
    params.grid_axis_widths = DirectX::XMFLOAT4(0.5f, 0.5f, 0.1f, 1000.0f);
    params.grid_plane_params = DirectX::XMFLOAT4(1.0f / 10.0f, 1.0f / 1.0f, 1.5f, 0.0f);
    params.grid_fade_params = DirectX::XMFLOAT4(0.7f, 1.0f, 0.0f, 0.0f);

    Shader_InfiniteGrid_SetGridParams(params);

    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->Draw(6, 0);
}


