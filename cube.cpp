#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "shader_3d.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	//XMFLOAT2 uv;
};

namespace {
	constexpr int NUM_VERTEX = 6 * 2 * 3; // 頂点数
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;

	Vertex3D g_CubeVertex[36]
	{
		{{-0.5f, 0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,1.0f,1.0f,1.0f}},
	};
}



void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;
	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	//bd.Usage = D3D11_USAGE_DYNAMIC;// 書き込み可能に設定
	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Cube_Draw(void)
{
	// シェーダーを描画パイプラインに設定
	Shader_3D_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//ワールド座標変換行列を設定
	XMMATRIX mtxWorld = XMMatrixIdentity();// 単位行列

	Shader_3D_SetWorldMatrix(mtxWorld);

	//ビュー座標変換行列を設定
	XMMATRIX mtxView = XMMatrixLookAtLH(
		{2.0f,2.0f,-5.0f} ,// 視点座標
		{0.0f,0.0f,0.0f}, // 注視点座標
		{0.0f,1.0f,0.0f} // 上方向ベクトル
	);


	float fovAngleY = XMConvertToRadians(60.0f);
	float aspectRatio = static_cast<float>(Direct3D_GetBackBufferWidth()) / static_cast<float>(Direct3D_GetBackBufferHeight());
	float nearZ = 0.1f;
	float farZ = 100.0f;

	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(
		fovAngleY,
		aspectRatio,
		nearZ,
		farZ
	);

	Shader_3D_SetProjectMatrix(mtxPerspective);
	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//g_pContext->PSSetShaderResources(0, 1, &g_pTexture);

	// ポリゴン描画命令発行
	g_pContext->Draw(6, 0);
}
