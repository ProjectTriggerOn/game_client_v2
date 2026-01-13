#include "shader_3d_unlit.h"
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>

namespace
{
	ID3D11VertexShader* g_pVertexShader = nullptr;
	ID3D11InputLayout* g_pInputLayout = nullptr;
	ID3D11Buffer* g_pVSConstantBuffer0 = nullptr;
	ID3D11Buffer* g_pPSConstantBuffer0 = nullptr;
	ID3D11PixelShader* g_pPixelShader = nullptr;

}
bool Shader_3DUnlit_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	HRESULT hr; // HRESULTはDirectXの関数の戻り値で、成功か失敗かを示す	

	//
	std::ifstream ifs_vs("resource/shader/shader_vertex_3dunlit.cso", std::ios::binary);

	if (!ifs_vs) {
		MessageBox(nullptr, "", "error", MB_OK);
		return false;
	}

	//
	ifs_vs.seekg(0, std::ios::end); //
	std::streamsize filesize = ifs_vs.tellg(); // 
	ifs_vs.seekg(0, std::ios::beg); //

	//
	unsigned char* vsbinary_pointer = new unsigned char[filesize];

	ifs_vs.read((char*)vsbinary_pointer, filesize); //
	ifs_vs.close(); //

	//
	hr = Direct3D_GetDevice()->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);

	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Initialize()" << std::endl;
		delete[] vsbinary_pointer; //
		return false;
	}

	//頂点レイアウトの定義
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,          0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	UINT num_elements = ARRAYSIZE(layout); // 

	//頂点レイアウトの作成
	hr = Direct3D_GetDevice()->CreateInputLayout(layout, num_elements, vsbinary_pointer, filesize, &g_pInputLayout);

	delete[] vsbinary_pointer; //

	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Initialize() :" << std::endl;
		return false;
	}

	// 
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4); // 
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; //

	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer0);

	//
	std::ifstream ifs_ps("resource/shader/shader_pixel_3dunlit.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "\n\nshader_pixel_billboard.cso", "error", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* psbinary_pointer = new unsigned char[filesize];
	ifs_ps.read((char*)psbinary_pointer, filesize);
	ifs_ps.close();

	//
	hr = Direct3D_GetDevice()->CreatePixelShader(psbinary_pointer, filesize, nullptr, &g_pPixelShader);

	delete[] psbinary_pointer; //

	if (FAILED(hr)) {
		hal::dout << "Shader_Billboard_Initialize() Error" << std::endl;
		return false;
	}

	//pixel shader constant buffer
	buffer_desc.ByteWidth = sizeof(XMFLOAT4); // 
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; //

	Direct3D_GetDevice()->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer0);

	return true;
}

void Shader_3DUnlit_Finalize()
{
	SAFE_RELEASE(g_pVertexShader);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pPSConstantBuffer0);
}

void Shader_3DUnlit_SetMatrix(const DirectX::XMMATRIX& matrix)
{
	//
	XMFLOAT4X4 transpose;

	//
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	//
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader_3DUnlit_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	//
	XMFLOAT4X4 transpose;

	//
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	//
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader_3DUnlit_SetViewMatrix(const DirectX::XMMATRIX& matrix)
{
}

void Shader_3DUnlit_SetProjectMatrix(const DirectX::XMMATRIX& matrix)
{
}

void Shader_3DUnlit_SetColor(const DirectX::XMFLOAT4& color)
{
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
}

void Shader_3DUnlit_Begin()
{
	//頂点シェーダーとピクセルシェーダーを描画パイプラインに設定
	Direct3D_GetDeviceContext()->VSSetShader(g_pVertexShader, nullptr, 0);
	Direct3D_GetDeviceContext()->PSSetShader(g_pPixelShader, nullptr, 0);

	//頂点レイアウトを描画パイプラインに設定
	Direct3D_GetDeviceContext()->IASetInputLayout(g_pInputLayout);

	//定数バッファを描画パイプラインに設定
	Direct3D_GetDeviceContext()->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer0);
	Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);
}
