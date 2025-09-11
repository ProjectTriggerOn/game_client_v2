#include <DirectXMath.h>
#include "grid.h"

#include "direct3d.h"
#include "shader_3d.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
};

namespace {
	constexpr int GRID_H_COUNT = 20;	// Horizontal lines count
	constexpr int GRID_V_COUNT = 20;	// Vertical lines count
	constexpr int GRID_H_LINE_COUNT = GRID_H_COUNT + 1;
	constexpr int GRID_V_LINE_COUNT = GRID_V_COUNT + 1;
	constexpr int NUM_VERTEX = (GRID_H_LINE_COUNT + GRID_V_LINE_COUNT) * 2;	// Total number of vertices

	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;

	Vertex3D g_CubeVertex[NUM_VERTEX]{};
}

static void Grid_CreateVertexes(float gridSize)
{
	int index = 0;
	for (int i = 0; i <= GRID_H_COUNT; ++i) {
		float z = (i - GRID_H_COUNT / 2) * gridSize;
		g_CubeVertex[index++] = {
			XMFLOAT3(-GRID_V_COUNT / 2 * gridSize, 0.0f, z),
			XMFLOAT4(1.0f,1.0f,1.0f, 1.0f) };
		g_CubeVertex[index++] = {
			XMFLOAT3(GRID_V_COUNT / 2 * gridSize, 0.0f, z),
			XMFLOAT4(1.0f,1.0f,1.0f, 1.0f) };
	}
	for (int i = 0; i <= GRID_V_COUNT; ++i) {
		float x = (i - GRID_V_COUNT / 2) * gridSize;
		g_CubeVertex[index++] = {
			XMFLOAT3(x, 0.0f, -GRID_H_COUNT / 2 * gridSize),
			XMFLOAT4(1.0f,1.0f,1.0f, 1.0f) };
		g_CubeVertex[index++] = {
			XMFLOAT3(x, 0.0f, GRID_H_COUNT / 2 * gridSize),
			XMFLOAT4(1.0f,1.0f,1.0f, 1.0f) };
	}
}

void Grid_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
	Grid_CreateVertexes(1.0f);
	// デバイスとデバイスコンテキストの保存
	g_pDevice = pDevice;
	g_pContext = pContext;
	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(Vertex3D) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = g_CubeVertex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pVertexBuffer);

}

void Grid_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Grid_Draw(void)
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


	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// ポリゴン描画命令発行
	g_pContext->Draw(NUM_VERTEX, 0);
}
