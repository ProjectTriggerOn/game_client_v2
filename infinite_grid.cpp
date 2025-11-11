
#include <d3d11.h>
#include <DirectXMath.h>
#include "shader_infinite.h"
#include "sampler.h"
#include "texture.h"
#include "infinite_grid.h"

using namespace DirectX;

namespace {
	// 如果有贴图需求可在此管理
	int g_infiniteGridTexId = -1;
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
}

void InfiniteGrid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pContext = pContext;
	g_pDevice = pDevice;
}

void InfiniteGrid_Finalize()
{
	Shader_InfiniteGrid_Finalize();
}

void InfiniteGrid_Draw(const DirectX::XMMATRIX& mtxWorld)
{
	// 这里仅需设置 view/proj/inv_view/inv_proj
	// 请由调用方自己设置这些参数（例：传入matrix或全局Camera参数等）
	// 
	// 例如：
	// Shader_InfiniteGrid_SetViewMatrix(viewMtx);
	// Shader_InfiniteGrid_SetProjectMatrix(projMtx);
	// Shader_InfiniteGrid_SetInvViewMatrix(invViewMtx);
	// Shader_InfiniteGrid_SetInvProjMatrix(invProjMtx);
	Shader_InfiniteGrid_UpdateCB();

	Shader_InfiniteGrid_Begin();

	Sampler_SetFilterLinear();
	// 贴图可选：
	// if (g_infiniteGridTexId >= 0) Texture_Set(g_infiniteGridTexId, 0);

	// 拓扑必须 TRIANGLELIST
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	// 不需要 IASetInputLayout / IASetVertexBuffers
	g_pContext->Draw(6, 0); // 用 SV_VertexID 画出两个三角形的 plane
}
