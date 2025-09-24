#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "shader_3d.h"
#include "key_logger.h"
#include "mouse.h"
#include "texture.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	XMFLOAT2 uv; // uv座標
};

namespace {
	int g_CubeTexId = -1; // テクスチャID

	constexpr int NUM_VERTEX = 24; // 頂点数
	constexpr int NUM_INDEX = 36;
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
	ID3D11Buffer* g_pIndexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	float g_RotationX = 0.0f;
	float g_RotationY = 0.0f;
	float g_RotationZ = 0.0f;

	XMFLOAT3 g_TranslationPosition = { 0.0f, 0.0f, 0.0f };

	XMFLOAT3 g_Scaling = { 1.0f, 1.0f, 1.0f };

	constexpr float SCALE_SPEED = 0.5f;

	Vertex3D g_CubeVertex[]
	{
		// 前面 (z=-0.5f) 红色
		{{-0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.0f, 0.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.25f, 0.5f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.0f, 0.5f}},
//0		{{-0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.0f, 0.0f}},
//2		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.25f, 0.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}, {0.25f, 0.5f}},
		// 后面 (z=0.5f) 绿色
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.5f, 0.0f}},//11
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.5f, 0.5f}},//10
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.25f, 0.5f}},//00
//		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.5f, 0.0f}},//11
//		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.25f, 0.5f}},//00
		{{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}, {0.25f, 0.0f}},//01
		// 左面 (x=-0.5f) 蓝色
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.75f, 0.0f}},//11
		{{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.75f, 0.5f}},//10
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.5f, 0.5f}},//00
//		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.75f, 0.0f}},//11
//		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.5f, 0.5f}},//00
		{{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}, {0.5f, 0.0f}},//01
		// 右面 (x=0.5f) 黄色
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f},{0.25f, 0.5f}},//11
		{{ 0.5f, 0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f},{0.25f, 1.0f}},//10
		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f},{0.0f, 1.0f}},//00
//		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f},{0.25f, 0.5f}},//11
//		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f},{0.0f, 1.0f}},//00
		{{ 0.5f,-0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f},{0.0f, 0.5f}},//01
		// 上面 (y=0.5f) 品红色
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f},{0.5f,  0.5f}},//11
		{{-0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f},{0.5f,  1.0f}},//10
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f},{0.25f,1.0f}},//00
//		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f},{0.5f,  0.5f}},//11
//		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f},{0.25f,1.0f}},//00
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f},{0.25f,0.5f}},//01
		// 下面 (y=-0.5f) 青色
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f},{0.75f,  0.5f}},//
		{{-0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f},{0.75f,  1.0f}},//
		{{ 0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f},{0.5f, 1.0f}},//
//		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f},{0.75f,  0.5f}},//
//		{{ 0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f},{0.5f, 1.0f}},//
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f},{0.5f, 0.5f}},//
	};
	unsigned short g_CubeVertexIndex[36]
	{
		0,1, 2, 0, 2, 3,       // 前面

		4,5, 6, 4, 6, 7,       // 後面
		8,9,10, 8,10,11,       // 左面
		12,13,14,12,14,15,     // 右面
	   16,17,18,16,18,19,     // 上面
	   20,21,22,20,22,23      // 下面
		
	};
}



void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{
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

	bd.Usage = D3D11_USAGE_DEFAULT;// 書き込み不可に設定
	bd.ByteWidth = sizeof(unsigned short) * NUM_INDEX;
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	sd.pSysMem = g_CubeVertexIndex;

	g_pDevice->CreateBuffer(&bd, &sd, &g_pIndexBuffer);

	g_CubeTexId = Texture_LoadFromFile(L"resource/texture/cube_tex.png");
}

void Cube_Finalize(void)
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Cube_Draw(XMMATRIX mtxW)
{
	// シェーダーを描画パイプラインに設定
	Shader_3D_Begin();

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex3D);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	Texture_Set(g_CubeTexId);

	Shader_3D_SetWorldMatrix(mtxW);

	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ポリゴン描画命令発行
	g_pContext->DrawIndexed(36, 0,0);
}


void Cube_Update(double)
{

}
