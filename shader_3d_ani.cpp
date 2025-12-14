#include <d3d11.h>
#include <DirectXMath.h>
#include <fstream>
#include <vector>
#include "shader_3d_ani.h"
#include "direct3d.h"
#include "debug_ostream.h"

using namespace DirectX;

namespace
{
	ID3D11VertexShader* g_pVertexShader = nullptr;
	ID3D11InputLayout* g_pInputLayout = nullptr;
	ID3D11Buffer* g_pVSConstantBuffer0 = nullptr; // World
	ID3D11Buffer* g_pVSConstantBuffer3 = nullptr; // Bones
	ID3D11Buffer* g_pPSConstantBuffer0 = nullptr; // Material Color
	ID3D11PixelShader* g_pPixelShader = nullptr;

	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;

	struct BoneBuffer
	{
		XMFLOAT4X4 boneTransforms[256];
	};
}

bool Shader_3D_Ani_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	HRESULT hr;

	if (!pDevice || !pContext) {
		hal::dout << "Shader_3D_Ani_Initialize() : ERROR" << std::endl;
		return false;
	}

	g_pDevice = pDevice;
	g_pContext = pContext;

	// Load Vertex Shader
	std::ifstream ifs_vs("resource/shader/shader_vertex_3d_ani.cso", std::ios::binary);
	if (!ifs_vs) {
		MessageBox(nullptr, "shader_vertex_3d_ani.cso not found", "error", MB_OK);
		return false;
	}

	ifs_vs.seekg(0, std::ios::end);
	std::streamsize filesize = ifs_vs.tellg();
	ifs_vs.seekg(0, std::ios::beg);

	std::vector<unsigned char> vsbinary(filesize);
	ifs_vs.read((char*)vsbinary.data(), filesize);
	ifs_vs.close();

	hr = g_pDevice->CreateVertexShader(vsbinary.data(), filesize, nullptr, &g_pVertexShader);
	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Ani_Initialize() CreateVertexShader failed" << std::endl;
		return false;
	}

	// Input Layout
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "BONES",    0, DXGI_FORMAT_R32G32B32A32_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	hr = g_pDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsbinary.data(), filesize, &g_pInputLayout);
	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Ani_Initialize() CreateInputLayout failed" << std::endl;
		return false;
	}

	// Constant Buffers
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4);
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	buffer_desc.Usage = D3D11_USAGE_DEFAULT;

	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer0);

	// Bone Buffer
	buffer_desc.ByteWidth = sizeof(BoneBuffer);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer3);

	// Load Pixel Shader
	std::ifstream ifs_ps("resource/shader/shader_pixel_3d_ani.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "shader_pixel_3d_ani.cso not found", "error", MB_OK);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	std::vector<unsigned char> psbinary(filesize);
	ifs_ps.read((char*)psbinary.data(), filesize);
	ifs_ps.close();

	hr = g_pDevice->CreatePixelShader(psbinary.data(), filesize, nullptr, &g_pPixelShader);
	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Ani_Initialize() CreatePixelShader failed" << std::endl;
		return false;
	}

	// PS Constant Buffer
	buffer_desc.ByteWidth = sizeof(XMFLOAT4);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pPSConstantBuffer0);

	return true;
}

void Shader_3D_Ani_Finalize()
{
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pVSConstantBuffer0) g_pVSConstantBuffer0->Release();
	if (g_pVSConstantBuffer3) g_pVSConstantBuffer3->Release();
	if (g_pPSConstantBuffer0) g_pPSConstantBuffer0->Release();
	if (g_pInputLayout) g_pInputLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
}

void Shader_3D_Ani_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	XMFLOAT4X4 transpose;
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));
	g_pContext->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader_3D_Ani_SetViewMatrix(const DirectX::XMMATRIX& matrix)
{
}

void Shader_3D_Ani_SetProjectMatrix(const DirectX::XMMATRIX& matrix)
{
}

void Shader_3D_Ani_SetBoneMatrices(const DirectX::XMFLOAT4X4* matrices, int count)
{
	if (count > 256) count = 256;
	
	BoneBuffer cb;
	ZeroMemory(&cb, sizeof(cb));
	
	for(int i=0; i<count; ++i) {
		XMMATRIX m = XMLoadFloat4x4(&matrices[i]);
		XMStoreFloat4x4(&cb.boneTransforms[i], XMMatrixTranspose(m));
	}
	
	g_pContext->UpdateSubresource(g_pVSConstantBuffer3, 0, nullptr, &cb, 0, 0);
}

void Shader_3D_Ani_SetColor(const DirectX::XMFLOAT4& color)
{
	g_pContext->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
}

void Shader_3D_Ani_Begin()
{
	g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);
	g_pContext->IASetInputLayout(g_pInputLayout);
	g_pContext->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer0);
	g_pContext->VSSetConstantBuffers(3, 1, &g_pVSConstantBuffer3);
	g_pContext->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);
}
