#include "shader_billboard.h"
#include <DirectXMath.h>
#include <d3d11.h>

#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>

namespace
{
	ID3D11VertexShader* g_pVertexShader = nullptr;
	ID3D11InputLayout* g_pInputLayout = nullptr;
	ID3D11Buffer* g_pVSConstantBuffer0 = nullptr; // world
	ID3D11Buffer* g_pVSConstantBuffer3 = nullptr; // UV scale/offset
	ID3D11Buffer* g_pPSConstantBuffer0 = nullptr; // color tint
	ID3D11PixelShader* g_pPixelShader = nullptr;
}

bool Shader_Billboard_Initialize()
{
	HRESULT hr;

	std::ifstream ifs_vs("resource/shader/shader_vertex_billboard.cso", std::ios::binary);
	if (!ifs_vs)
	{
		MessageBox(nullptr, "Failed to load the vertex shader.\n\nshader_vertex_billboard.cso", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	ifs_vs.seekg(0, std::ios::end);
	std::streamsize filesize = ifs_vs.tellg();
	ifs_vs.seekg(0, std::ios::beg);

	unsigned char* vsbinary_pointer = new unsigned char[filesize];
	ifs_vs.read((char*)vsbinary_pointer, filesize);
	ifs_vs.close();

	hr = Direct3D_GetDevice()->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);
	if (FAILED(hr))
	{
		hal::dout << "Shader_Billboard_Initialize() : failed to create the vertex shader" << std::endl;
		delete[] vsbinary_pointer;
		return false;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = ARRAYSIZE(layout);

	hr = Direct3D_GetDevice()->CreateInputLayout(layout, numElements, vsbinary_pointer, filesize, &g_pInputLayout);
	delete[] vsbinary_pointer;
	if (FAILED(hr))
	{
		hal::dout << "Shader_Billboard_Initialize() : failed to create the input layout" << std::endl;
		return false;
	}

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT4X4);
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Direct3D_GetDevice()->CreateBuffer(&bufferDesc, nullptr, &g_pVSConstantBuffer0);

	bufferDesc.ByteWidth = sizeof(UVParameter);
	Direct3D_GetDevice()->CreateBuffer(&bufferDesc, nullptr, &g_pVSConstantBuffer3);

	std::ifstream ifs_ps("resource/shader/shader_pixel_billboard.cso", std::ios::binary);
	if (!ifs_ps)
	{
		MessageBox(nullptr, "Failed to load the pixel shader.\n\nshader_pixel_billboard.cso", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	ifs_ps.seekg(0, std::ios::end);
	filesize = ifs_ps.tellg();
	ifs_ps.seekg(0, std::ios::beg);

	unsigned char* psbinary_pointer = new unsigned char[filesize];
	ifs_ps.read((char*)psbinary_pointer, filesize);
	ifs_ps.close();

	hr = Direct3D_GetDevice()->CreatePixelShader(psbinary_pointer, filesize, nullptr, &g_pPixelShader);
	delete[] psbinary_pointer;
	if (FAILED(hr))
	{
		hal::dout << "Shader_Billboard_Initialize() : failed to create the pixel shader" << std::endl;
		return false;
	}

	bufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT4);
	Direct3D_GetDevice()->CreateBuffer(&bufferDesc, nullptr, &g_pPSConstantBuffer0);

	return true;
}

void Shader_Billboard_Finalize()
{
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pVSConstantBuffer3);
	SAFE_RELEASE(g_pPSConstantBuffer0);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVertexShader);
}

void Shader_Billboard_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	DirectX::XMFLOAT4X4 transpose;
	DirectX::XMStoreFloat4x4(&transpose, DirectX::XMMatrixTranspose(matrix));
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader_Billboard_SetViewMatrix([[maybe_unused]] const DirectX::XMMATRIX& matrix)
{
	// b1 is bound globally by Camera_SetMatrixToShader every frame.
}

void Shader_Billboard_SetProjectMatrix([[maybe_unused]] const DirectX::XMMATRIX& matrix)
{
	// b2 is bound globally by Camera_SetMatrixToShader every frame.
}

void Shader_Billboard_SetColor(const DirectX::XMFLOAT4& color)
{
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pPSConstantBuffer0, 0, nullptr, &color, 0, 0);
}

void Shader_Billboard_SetUVParameter(const UVParameter& parameter)
{
	Direct3D_GetDeviceContext()->UpdateSubresource(g_pVSConstantBuffer3, 0, nullptr, &parameter, 0, 0);
}

void Shader_Billboard_Begin()
{
	Direct3D_GetDeviceContext()->VSSetShader(g_pVertexShader, nullptr, 0);
	Direct3D_GetDeviceContext()->PSSetShader(g_pPixelShader, nullptr, 0);
	Direct3D_GetDeviceContext()->IASetInputLayout(g_pInputLayout);
	Direct3D_GetDeviceContext()->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer0);
	Direct3D_GetDeviceContext()->VSSetConstantBuffers(3, 1, &g_pVSConstantBuffer3);
	Direct3D_GetDeviceContext()->PSSetConstantBuffers(0, 1, &g_pPSConstantBuffer0);
}
