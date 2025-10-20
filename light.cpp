#include "light.h"
using namespace DirectX;
namespace 
{
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	ID3D11Buffer * g_pVSConstantBuffer3 = nullptr;
	ID3D11Buffer* g_pVSConstantBuffer4= nullptr;
}

struct DirectionalLight
{
	XMFLOAT4 direction;
	XMFLOAT4 color;
};

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; //


	buffer_desc.ByteWidth = sizeof(XMFLOAT4); // 
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer3);

	buffer_desc.ByteWidth = sizeof(DirectionalLight); // 
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer4);
}

void Light_Finalize(void)
{
}

void Light_SetAmbient(const XMFLOAT3& color)
{
	//定数バッファに転送
	g_pContext->UpdateSubresource(g_pVSConstantBuffer3, 0, nullptr, &color, 0, 0);
	g_pContext->VSSetConstantBuffers(3, 1, &g_pVSConstantBuffer3);

}

void Light_SetDirectionalWorld(const DirectX::XMFLOAT4& direction, const DirectX::XMFLOAT4& color)
{
	DirectionalLight light{
		direction,
		color
	};

	//定数バッファに転送
	g_pContext->UpdateSubresource(g_pVSConstantBuffer4, 0, nullptr, &light, 0, 0);
	g_pContext->VSSetConstantBuffers(4, 1, &g_pVSConstantBuffer4);
}
