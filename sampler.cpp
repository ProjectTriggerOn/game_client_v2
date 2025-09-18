#include "sampler.h"

#include "direct3d.h"

namespace
{
	ID3D11SamplerState* g_pSamplerPoint = nullptr;
	ID3D11SamplerState* g_pSamplerLinear = nullptr;
	ID3D11SamplerState* g_pSamplerAnisotropic = nullptr;

	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
}

void Sampler_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_pDevice = pDevice;
	g_pContext = pContext;

	D3D11_SAMPLER_DESC sampler_desc{};

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

	//UV参照外のテクスチャのアドレスモードを設定
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // U軸のアドレスモード
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; // V軸のアドレスモード

	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // W軸のアドレスモード
	sampler_desc.MipLODBias = 0.0f; // MIPレベルのバイアス
	sampler_desc.MaxAnisotropy = 16; // 異方性フィルタリングの最大値
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS; // 比較関数
	sampler_desc.MinLOD = 0.0f; // 最小LOD
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX; // 最大LOD

	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerPoint);

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; // 線形フィルタリング
	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerLinear);

	sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC; // 異方性フィルタリング
	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerAnisotropic);
}

void Sampler_Finalize()
{
	SAFE_RELEASE(g_pSamplerPoint)
	SAFE_RELEASE(g_pSamplerLinear)
	SAFE_RELEASE(g_pSamplerAnisotropic)
}

void Sampler_SetFilterPoint()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerPoint);
}

void Sampler_SetFilterLinear()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerLinear);
}

void Sampler_SetFilterAnisotropic()
{
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerAnisotropic);
}
