#include "collision.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "texture.h"
using namespace DirectX;



bool Collision_OverlapCircleCircle(const Circle& a, const Circle& b)
{
	float x1 = b.center.x - a.center.x;
	float y1 = b.center.y - a.center.y;

	return (a.radius + b.radius) * (a.radius + b.radius) > (x1 * x1 + y1 * y1);

	//XMVECTOR centerA = XMLoadFloat2(&a.center);
	//XMVECTOR centerB = XMLoadFloat2(&b.center);
	//XMVECTOR lsq = XMVector2LengthSq(centerB - centerA);

	//return (a.radius + b.radius) * (a.radius + b.radius) > XMVectorGetX(lsq);
}

bool Collision_OverlapCircleBox(const Box& a, const Box& b)
{
	float at = a.center.y - a.halfHeight;
	float ab = a.center.y + a.halfHeight;
	float al = a.center.x - a.halfWidth;
	float ar = a.center.x + a.halfWidth;
	float bt = b.center.y - b.halfHeight;
	float bb = b.center.y + b.halfHeight;
	float bl = b.center.x - b.halfWidth;
	float br = b.center.x + b.halfWidth;

	return al < br && ar > bl && at < bb && ab > bt;
}


static constexpr int NUM_VERTEX = 5000; // 頂点数の上限

namespace
{
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	int g_WhiteId = -1; // 白色のテクスチャID
}

struct Vertex
{
	XMFLOAT3 position; // 頂点座標
	XMFLOAT4 color;
	XMFLOAT2 uv;
};

void Collision_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
{

	g_WhiteId = Texture_LoadFromFile(L"resource/texture/white.png"); // 白色のテクスチャを読み込む
	g_pDevice = pDevice;
	g_pContext = pContext;

	// 頂点バッファ生成
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Vertex) * NUM_VERTEX;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);

}

void Collision_DebugFinalize()
{
	SAFE_RELEASE(g_pVertexBuffer);
}

void Collision_DebugDraw(const Circle& circle,const DirectX::XMFLOAT4& color)
{
	int vertexNum = static_cast<int>(circle.radius * XM_2PI + 1);

	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	const float rad = 2.0f * DirectX::XM_PI / vertexNum; // 1頂点あたりの角度

	for (int i = 0; i < vertexNum; ++i) {
		float angle = rad * i; // 現在の角度
		v[i].position = {
			circle.center.x + circle.radius * cosf(angle),
			circle.center.y + circle.radius * sinf(angle),
			0.0f
		};
		v[i].color = color;
		v[i].uv = { 0.0f,0.0f };
	}

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(XMMatrixIdentity());


	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	Texture_Set(g_WhiteId); // 白色のテクスチャを設定;

	g_pContext->Draw(vertexNum, 0);
}

void Collision_DebugDraw(const Box& box, const DirectX::XMFLOAT4& color)
{

	// シェーダーを描画パイプラインに設定
	Shader_Begin();

	// 頂点バッファをロックする
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 頂点バッファへの仮想ポインタを取得
	Vertex* v = (Vertex*)msr.pData;

	v[0].position = {box.center.x - box.halfWidth,box.center.y - box.halfHeight,0.0f};
	v[1].position = { box.center.x +box.halfWidth,box.center.y - box.halfHeight,0.0f };
	v[2].position= { box.center.x + box.halfWidth,box.center.y + box.halfHeight,0.0f };
	v[3].position= { box.center.x - box.halfWidth,box.center.y + box.halfHeight,0.0f };

	for (int i = 0; i < 4; ++i) {
		
		v[i].color = color;
		v[i].uv = { 0.0f,0.0f };
	}

	// 頂点バッファのロックを解除
	g_pContext->Unmap(g_pVertexBuffer, 0);

	// 頂点バッファを描画パイプラインに設定
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(XMMatrixIdentity());


	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	Texture_Set(g_WhiteId); // 白色のテクスチャを設定;

	g_pContext->Draw(4, 0);
}
