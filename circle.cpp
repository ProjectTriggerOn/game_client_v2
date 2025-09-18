#include "circle.h"

#include "direct3d.h"
#include "shader_3d.h"
#include "texture.h"
using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
};

namespace
{
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	int g_WhiteId = -1; // 白色のテクスチャID
	constexpr int NUM_VERTEX = 5000; // 頂点数の上限
	Vertex3D g_CubeVertex[NUM_VERTEX]{};
}



void Circle_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	g_WhiteId = Texture_LoadFromFile(L"resource/texture/white.png"); // 白色のテクスチャを読み込む

	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;


	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);
}

void Circle_DebugFinalize()
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Circle_DebugDraw(const CircleD& circle, const DirectX::XMFLOAT4& color)
{
	int vertexNum = static_cast<int>(circle.radius * XM_2PI + 1);

	const float rad = 2.0f * DirectX::XM_PI / vertexNum; // 1頂点あたりの角度

	for (int i = 0; i < vertexNum; ++i) {
		float angle = rad * i; // 現在の角度
		g_CubeVertex[i].position = {
			circle.center.x + circle.radius * cosf(angle),
			0.0f,
			circle.center.y + circle.radius * sinf(angle),
			
		};
		g_CubeVertex[i].color = color;
	}

	// シェーダーを描画パイプラインに設定
	Shader_3D_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	//ワールド座標変換行列を設定
	XMMATRIX mtxWorld = XMMatrixIdentity();// 単位行列
	Shader_3D_SetWorldMatrix(mtxWorld);


	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	// ポリゴン描画命令発行
	g_pContext->Draw(vertexNum, 0);
}
