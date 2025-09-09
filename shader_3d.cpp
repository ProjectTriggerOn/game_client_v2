#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "direct3d.h"
#include "debug_ostream.h"
#include <fstream>
#include "shader_3d.h"


static ID3D11VertexShader* g_pVertexShader = nullptr;
static ID3D11InputLayout* g_pInputLayout = nullptr;
static ID3D11Buffer* g_pVSConstantBuffer0 = nullptr;static ID3D11Buffer* g_pVSConstantBuffer1 = nullptr;
static ID3D11PixelShader* g_pPixelShader = nullptr;
static ID3D11SamplerState* g_pSamplerState = nullptr;

// 注意！初期化で外部から設定されるもの。Release不要。
static ID3D11Device* g_pDevice = nullptr;
static ID3D11DeviceContext* g_pContext = nullptr;


bool Shader_3D_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	HRESULT hr; // HRESULTはDirectXの関数の戻り値で、成功か失敗かを示す	

	// デバイスとデバイスコンテキストのチェック
	if (!pDevice || !pContext) {
		hal::dout << "Shader_3D_Initialize() : �^����ꂽ�f�o�C�X���R���e�L�X�g���s���ł�" << std::endl;
		return false;
	}

	// �f�o�C�X�ƃf�o�C�X�R���e�L�X�g�̕ۑ�
	g_pDevice = pDevice;
	g_pContext = pContext;


	// ���O�R���p�C���ςݒ��_�V�F�[�_�[�̓ǂݍ���
	std::ifstream ifs_vs("resource/shader/shader_vertex_2d.cso", std::ios::binary);

	if (!ifs_vs) {
		MessageBox(nullptr, "���_�V�F�[�_�[�̓ǂݍ��݂Ɏ��s���܂���\n\nshader_vertex_2d.cso", "�G���[", MB_OK);
		return false;
	}

	// �t�@�C���T�C�Y��擾
	ifs_vs.seekg(0, std::ios::end); // �t�@�C���|�C���^�𖖔��Ɉړ�
	std::streamsize filesize = ifs_vs.tellg(); // �t�@�C���|�C���^�̈ʒu��擾�i�܂�t�@�C���T�C�Y�j
	ifs_vs.seekg(0, std::ios::beg); // �t�@�C���|�C���^��擪�ɖ߂�

	// �o�C�i���f�[�^��i�[���邽�߂̃o�b�t�@��m��
	unsigned char* vsbinary_pointer = new unsigned char[filesize];
	
	ifs_vs.read((char*)vsbinary_pointer, filesize); // �o�C�i���f�[�^��ǂݍ���
	ifs_vs.close(); // �t�@�C�������

	// ���_�V�F�[�_�[�̍쐬
	hr = g_pDevice->CreateVertexShader(vsbinary_pointer, filesize, nullptr, &g_pVertexShader);

	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Initialize() : ���_�V�F�[�_�[�̍쐬�Ɏ��s���܂���" << std::endl;
		delete[] vsbinary_pointer; // ���������[�N���Ȃ��悤�Ƀo�C�i���f�[�^�̃o�b�t�@����
		return false;
	}


	//頂点レイアウトの定義
	D3D11_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	};

	UINT num_elements = ARRAYSIZE(layout); // �z��̗v�f����擾

	//頂点レイアウトの作成
	hr = g_pDevice->CreateInputLayout(layout, num_elements, vsbinary_pointer, filesize, &g_pInputLayout);

	delete[] vsbinary_pointer; // �o�C�i���f�[�^�̃o�b�t�@����

	if (FAILED(hr)) {
		hal::dout << "Shader_3D_Initialize() : ���_���C�A�E�g�̍쐬�Ɏ��s���܂���" << std::endl;
		return false;
	}


	// ���_�V�F�[�_�[�p�萔�o�b�t�@�̍쐬
	D3D11_BUFFER_DESC buffer_desc{};
	buffer_desc.ByteWidth = sizeof(XMFLOAT4X4); // �o�b�t�@�̃T�C�Y
	buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; // �o�C���h�t���O

	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer0);
	g_pDevice->CreateBuffer(&buffer_desc, nullptr, &g_pVSConstantBuffer1);


	//
	std::ifstream ifs_ps("resource/shader/shader_pixel_2d.cso", std::ios::binary);
	if (!ifs_ps) {
		MessageBox(nullptr, "�s�N�Z���V�F�[�_�[�̓ǂݍ��݂Ɏ��s���܂���\n\nshader_pixel_2d.cso", "�G���[", MB_OK);
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
		hal::dout << "Shader_3D_Initialize() Error" << std::endl;
		return false;
	}

	D3D11_SAMPLER_DESC sampler_desc{};

	sampler_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // 点フィルタリング

	//UV参照外のテクスチャのアドレスモードを設定
	sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP; // U軸のアドレスモード
	sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP; // V軸のアドレスモード

	sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; // W軸のアドレスモード
	sampler_desc.MipLODBias = 0.0f; // MIPレベルのバイアス
	sampler_desc.MaxAnisotropy = 8; // 異方性フィルタリングの最大値
	sampler_desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS; // 比較関数
	sampler_desc.MinLOD = 0.0f; // 最小LOD
	sampler_desc.MaxLOD = D3D11_FLOAT32_MAX; // 最大LOD


	g_pDevice->CreateSamplerState(&sampler_desc, &g_pSamplerState);

	return true;
}



void Shader_3D_Finalize()
{
	SAFE_RELEASE(g_pSamplerState);
	SAFE_RELEASE(g_pPixelShader);
	SAFE_RELEASE(g_pVSConstantBuffer0);
	SAFE_RELEASE(g_pVSConstantBuffer1);
	SAFE_RELEASE(g_pInputLayout);
	SAFE_RELEASE(g_pVertexShader);
}

void Shader_3D_SetMatrix(const DirectX::XMMATRIX& matrix)
{
	//
	XMFLOAT4X4 transpose;

	//
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	//
	g_pContext->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);
}

void Shader_3D_SetWorldMatrix(const DirectX::XMMATRIX& matrix)
{
	//
	XMFLOAT4X4 transpose;

	//
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	//
	g_pContext->UpdateSubresource(g_pVSConstantBuffer1, 0, nullptr, &transpose, 0, 0);

}

void Shader_3D_SetProjectMatrix(const DirectX::XMMATRIX& matrix)
{
	//
	XMFLOAT4X4 transpose;

	//
	XMStoreFloat4x4(&transpose, XMMatrixTranspose(matrix));

	//
	g_pContext->UpdateSubresource(g_pVSConstantBuffer0, 0, nullptr, &transpose, 0, 0);

}

void Shader_3D_Begin()
{
	//頂点シェーダーとピクセルシェーダーを描画パイプラインに設定
	g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
	g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);

	//頂点レイアウトを描画パイプラインに設定
	g_pContext->IASetInputLayout(g_pInputLayout);

	//定数バッファを描画パイプラインに設定
	g_pContext->VSSetConstantBuffers(0, 1, &g_pVSConstantBuffer0);
	g_pContext->VSSetConstantBuffers(1, 1, &g_pVSConstantBuffer1);

	//サンプラーステートを描画パイプラインに設定
	g_pContext->PSSetSamplers(0, 1, &g_pSamplerState);

	
}
