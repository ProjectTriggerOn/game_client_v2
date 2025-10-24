#include "Light.h"
using namespace DirectX;

namespace {
	ID3D11Buffer* g_pPSConstantBuffer1 = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	ID3D11Device* g_pDevice = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer2 = nullptr;
}

class DirectionalLight
{
public:
	XMFLOAT4 Directional;
	XMFLOAT4 Color;
	XMFLOAT4 CameraPosition;
};

void Light_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.ByteWidth = sizeof(XMFLOAT4);


	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1);

	buffer_desc.ByteWidth = sizeof(DirectionalLight);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer2);
}

void Light_Finalize(void)
{
	if (g_pPSConstantBuffer2) {
		g_pPSConstantBuffer2->Release();
		g_pPSConstantBuffer1 = nullptr;
	}
	if (g_pPSConstantBuffer1) {
		g_pPSConstantBuffer1->Release();
		g_pPSConstantBuffer1 = nullptr;
	}
}

void Light_SetAmbient(const DirectX::XMFLOAT3& color)
{
	g_pContext->UpdateSubresource(g_pPSConstantBuffer1, 0, nullptr, &color, 0, 0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);
}

void Light_SetDirectionalWorld(const DirectX::XMFLOAT4& world_dir, const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT3& CameraPosition)
{
	DirectionalLight light{
		world_dir,
		color,
		{
		CameraPosition.x,
		CameraPosition.y,
		CameraPosition.z,
		0.0f
		}
	};
	g_pContext->UpdateSubresource(g_pPSConstantBuffer2, 0, nullptr, &light, 0, 0);
	g_pContext->PSSetConstantBuffers(2, 1, &g_pPSConstantBuffer2);

}
