#include "shader_infinite.h"
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>

struct ViewTransform
{
	DirectX::XMFLOAT4X4 g_mView;
	DirectX::XMFLOAT4X4 g_mProj;
	DirectX::XMFLOAT4X4 g_m_inv_view;
	DirectX::XMFLOAT4X4 g_m_inv_proj;
};
struct GridParams
{
	DirectX::XMFLOAT4 grid_axis_widths;
	DirectX::XMFLOAT4 grid_plane_params;
	DirectX::XMFLOAT4 grid_fade_params;
};

namespace
{
	ID3D11VertexShader* g_pVertexShader = nullptr;
	ID3D11InputLayout* g_pInputLayout = nullptr;
	ID3D11Buffer* g_pConstantBuffer0 = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer1 = nullptr;
	ID3D11PixelShader* g_pPixelShader = nullptr;

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;

	ViewTransform g_viewTransform;
}



bool Shader_InfiniteGrid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	HRESULT hr; // HRESULTはDirectXの関数の戻り値で、成功か失敗かを示す	

	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Shader_Grid_Initialize() : ERROR" << std::endl;
		return false;
	}

	g_pDevice = pDevice;
	g_pContext = pContext;

	std::ifstream ifs_vs("resource/shader/infinite_grid_vertex.cso", std::ios::binary);

	if (!ifs_vs) {
		MessageBox(nullptr, "", "error", MB_OK);
		return false;
	}


	ifs_vs.seekg(0, std::ios::end);
	std::streamsize filesize = ifs_vs.tellg();
	ifs_vs.seekg(0, std::ios::beg);

	unsigned char* vsbinary_pointer = new unsigned char[filesize];

	ifs_vs.read((char*)vsbinary_pointer, filesize);
	ifs_vs.close();

	hr = g_pDevice->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);

	if (FAILED(hr)) {
		hal::dout << "Shader_Grid_Initialize()" << std::endl;
		delete[] vsbinary_pointer; //
		return false;
	}

	//頂点レイアウトの定義
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT num_elements = ARRAYSIZE(layout);

	//頂点レイアウトの作成
	hr = g_pDevice->CreateInputLayout(layout, num_elements, vsbinary_pointer, filesize, &g_pInputLayout);

	delete[] vsbinary_pointer;

	if (FAILED(hr)) {
		hal::dout << "Shader_Grid_Initialize() :" << std::endl;
		return false;
	}


	// 
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(ViewTransform); // 
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; //

	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pConstantBuffer0);

	buffer_desc.ByteWidth = sizeof(GridParams); // 
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; //

	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer1);


	//
	std::ifstream ifs_ps("resource/shader/infinite_grid_pixel.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "\n infinite_grid_pixel.cso", "error", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* psbinary_pointer = new unsigned char[filesize];
	ifs_ps.read((char*)psbinary_pointer, filesize);
	ifs_ps.close();

	//
	hr = g_pDevice->CreatePixelShader(psbinary_pointer, filesize, nullptr, &g_pPixelShader);

	delete[] psbinary_pointer; //

	if (FAILED(hr)) {
		hal::dout << "Shader_Grid_Initialize() Error" << std::endl;
		return false;
	}
	return true;
}

void Shader_InfiniteGrid_Finalize()
{
	if (g_pVertexShader) { g_pVertexShader->Release(); g_pVertexShader = nullptr; }
	if (g_pInputLayout) { g_pInputLayout->Release(); g_pInputLayout = nullptr; }
	if (g_pPSConstantBuffer1) { g_pPSConstantBuffer1->Release(); g_pPSConstantBuffer1 = nullptr; }
	if (g_pConstantBuffer0) { g_pConstantBuffer0->Release(); g_pConstantBuffer0 = nullptr; }
	if (g_pPixelShader) { g_pPixelShader->Release(); g_pPixelShader = nullptr; }
}

void Shader_InfiniteGrid_SetProjectMatrix(const DirectX::XMMATRIX& matrix)
{
	XMStoreFloat4x4(&g_viewTransform.g_mProj, matrix);
	XMMATRIX invProj = XMMatrixInverse(nullptr, matrix);
	XMStoreFloat4x4(&g_viewTransform.g_m_inv_proj, invProj);

	g_pContext->UpdateSubresource(g_pConstantBuffer0, 0, nullptr, &g_viewTransform, 0, 0);
}

void Shader_InfiniteGrid_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	XMStoreFloat4x4(&g_viewTransform.g_mView, matrix);
	XMMATRIX invView = XMMatrixInverse(nullptr, matrix);
	XMStoreFloat4x4(&g_viewTransform.g_m_inv_view, invView);

	g_pContext->UpdateSubresource(g_pConstantBuffer0, 0, nullptr, &g_viewTransform, 0, 0);
}

void Shader_InfiniteGrid_Begin()
{
	g_pContext->IASetInputLayout(g_pInputLayout);
	g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);

	g_pContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer0);
	g_pContext->PSSetConstantBuffers(0, 1, &g_pConstantBuffer0);
	g_pContext->PSSetConstantBuffers(1, 1, &g_pPSConstantBuffer1);

}

