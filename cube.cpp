#include "cube.h"

#include <DirectXMath.h>

#include "direct3d.h"
#include "shader_3d.h"
#include "key_logger.h"

using namespace DirectX;

struct Vertex3D
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
};

namespace {
	constexpr int NUM_VERTEX = 6 * 2 * 3; // 頂点数
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ

	// 注意！初期化で外部から設定されるもの。Release不要。
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	float g_RotationX = 0.0f;
	float g_RotationY = 0.0f;
	float g_RotationZ = 0.0f;

	Vertex3D g_CubeVertex[36]
	{
		// 前面 (z=-0.5f) 红色
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,0.0f,0.0f,1.0f}},
		// 后面 (z=0.5f) 绿色
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{0.0f,1.0f,0.0f,1.0f}},
		// 左面 (x=-0.5f) 蓝色
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,0.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,0.0f,1.0f,1.0f}},
		// 右面 (x=0.5f) 黄色
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{1.0f,1.0f,0.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{1.0f,1.0f,0.0f,1.0f}},
		// 上面 (y=0.5f) 品红色
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{-0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f, 0.5f},{1.0f,0.0f,1.0f,1.0f}},
		{{ 0.5f, 0.5f,-0.5f},{1.0f,0.0f,1.0f,1.0f}},
		// 下面 (y=-0.5f) 青色
		{{-0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{-0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f,-0.5f},{0.0f,1.0f,1.0f,1.0f}},
		{{ 0.5f,-0.5f, 0.5f},{0.0f,1.0f,1.0f,1.0f}}
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
	//XMMATRIX mtxWorld = XMMatrixIdentity();// 単位行列
	//Shader_3D_SetWorldMatrix(mtxWorld);
	XMMATRIX mtxWorld =
		XMMatrixRotationX(g_RotationX) *
		XMMatrixRotationY(g_RotationY) *
		XMMatrixRotationZ(g_RotationZ);
	Shader_3D_SetWorldMatrix(mtxWorld);

	



	// プリミティブトポロジ設定
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// ポリゴン描画命令発行
	g_pContext->Draw(36, 0);
}

void Cube_Update(double elapsed_time)
{

		const float ROT_SPEED = 1.0f; // 旋转速度，可以调整

		if (KeyLogger_IsPressed(KK_A)) g_RotationY -= (float)(ROT_SPEED * elapsed_time);
		if (KeyLogger_IsPressed(KK_D)) g_RotationY += (float)(ROT_SPEED * elapsed_time);
		if (KeyLogger_IsPressed(KK_W)) g_RotationX += (float)(ROT_SPEED * elapsed_time);
		if (KeyLogger_IsPressed(KK_S)) g_RotationX -= (float)(ROT_SPEED * elapsed_time);
		if (KeyLogger_IsPressed(KK_Q)) g_RotationZ += (float)(ROT_SPEED * elapsed_time);
		if (KeyLogger_IsPressed(KK_E)) g_RotationZ -= (float)(ROT_SPEED * elapsed_time);
	

}
