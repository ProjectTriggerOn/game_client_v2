#include "collision.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "texture.h"
#include <algorithm>
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

bool Collision_IsOverLapAABB(const AABB& a, const AABB& b)
{
	return a.min.x <= b.max.x
		&& a.max.x >= b.min.x
		&& a.min.y <= b.max.y
		&& a.max.y >= b.min.y
		&& a.min.z <= b.max.z
		&& a.max.z >= b.min.z;
}

Hit Collision_IsHitAABB(const AABB& a, const AABB& b)
{
	Hit hit{};
	hit.isHit = Collision_IsOverLapAABB(a, b);
	if (!hit.isHit) return hit;

	float xdepth = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
	float ydepth = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
	float zdepth = std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);

	DirectX::XMFLOAT3 aCenter = a.GetCenter();
	DirectX::XMFLOAT3 bCenter = b.GetCenter();
	DirectX::XMFLOAT3 normal = { 0,0,0 };

	if (xdepth <= ydepth && xdepth <= zdepth) {
		normal.x = (bCenter.x > aCenter.x) ? 1.0f : -1.0f;
	}
	else if (ydepth <= xdepth && ydepth <= zdepth) {
		normal.y = (bCenter.y > aCenter.y) ? 1.0f : -1.0f;
	}
	else {
		normal.z = (bCenter.z > aCenter.z) ? 1.0f : -1.0f;
	}
	hit.normal = normal;
	return hit;
}

bool Collision_OverlapSphere(const Sphere& sphere, const DirectX::XMFLOAT3& point)
{
	XMVECTOR centerA = XMLoadFloat3(&sphere.center);
	XMVECTOR centerB = XMLoadFloat3(&point);
	XMVECTOR lsq = XMVector3LengthSq(centerB - centerA);

	return sphere.radius * sphere.radius > XMVectorGetX(lsq);
}

bool Collision_isHitRayOnSphere(const Ray& ray, const Sphere& sphere, float* outDistance)
{
	XMVECTOR rayOrigin = XMLoadFloat3(&ray.origin);
	XMVECTOR rayDirection = XMLoadFloat3(&ray.direction);
	XMVECTOR sphereCenter = XMLoadFloat3(&sphere.center);
	XMVECTOR m = rayOrigin - sphereCenter;
	float b = XMVectorGetX(XMVector3Dot(m, rayDirection));
	float c = XMVectorGetX(XMVector3Dot(m, m)) - sphere.radius * sphere.radius;
	// 光線起点在球外且光线远离球心
	if (c > 0.0f && b > 0.0f) {
		return false;
	}
	float discriminant = b * b - c;
	// 判定なし
	if (discriminant < 0.0f) {
		return false;
	}
	// 衝突距离を計算
	float t = -b - sqrtf(discriminant);
	// 光線起点が球内部にある場合
	if (t < 0.0f) {
		t = 0.0f;
	}
	if (outDistance) {
		*outDistance = t;
	}
	return true;
}

bool Collision_OverlapSphere(const Sphere& sphereA, const Sphere& sphereB)
{
	XMVECTOR centerA = XMLoadFloat3(&sphereA.center);
	XMVECTOR centerB = XMLoadFloat3(&sphereB.center);
	XMVECTOR lsq = XMVector3LengthSq(centerB - centerA);

	return (sphereA.radius + sphereB.radius) * (sphereA.radius + sphereB.radius) > XMVectorGetX(lsq);
}


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

void Collision_DebugDraw(const Circle& circle, const DirectX::XMFLOAT4& color)
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

	v[0].position = { box.center.x - box.halfWidth,box.center.y - box.halfHeight,0.0f };
	v[1].position = { box.center.x + box.halfWidth,box.center.y - box.halfHeight,0.0f };
	v[2].position = { box.center.x + box.halfWidth,box.center.y + box.halfHeight,0.0f };
	v[3].position = { box.center.x - box.halfWidth,box.center.y + box.halfHeight,0.0f };

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

void Collision_DebugDraw(const AABB& aabb, const DirectX::XMFLOAT4& color)
{
	// 1. 获取AABB的8个顶点（角）
	DirectX::XMFLOAT3 corners[8] = {
		{ aabb.min.x, aabb.min.y, aabb.min.z }, // 0: Bottom-Back-Left
		{ aabb.max.x, aabb.min.y, aabb.min.z }, // 1: Bottom-Back-Right
		{ aabb.max.x, aabb.max.y, aabb.min.z }, // 2: Top-Back-Right
		{ aabb.min.x, aabb.max.y, aabb.min.z }, // 3: Top-Back-Left
		{ aabb.min.x, aabb.min.y, aabb.max.z }, // 4: Bottom-Front-Left
		{ aabb.max.x, aabb.min.y, aabb.max.z }, // 5: Bottom-Front-Right
		{ aabb.max.x, aabb.max.y, aabb.max.z }, // 6: Top-Front-Right
		{ aabb.min.x, aabb.max.y, aabb.max.z }  // 7: Top-Front-Left
	};

	// 2. 定义12条线 (需要24个顶点)
	Vertex v[24];

	auto set_line = [&](int v_index, int corner_a, int corner_b) {
		v[v_index].position = corners[corner_a];
		v[v_index].color = color;
		v[v_index].uv = { 0.0f, 0.0f };

		v[v_index + 1].position = corners[corner_b];
		v[v_index + 1].color = color;
		v[v_index + 1].uv = { 0.0f, 0.0f };
		};

	// 绘制底面 (4条线)
	set_line(0, 0, 1);
	set_line(2, 1, 5);
	set_line(4, 5, 4);
	set_line(6, 4, 0);

	// 绘制顶面 (4条线)
	set_line(8, 3, 2);
	set_line(10, 2, 6);
	set_line(12, 6, 7);
	set_line(14, 7, 3);

	// 绘制垂直的边 (4条线)
	set_line(16, 0, 3);
	set_line(18, 1, 2);
	set_line(20, 5, 6);
	set_line(22, 4, 7);


	// 3. 复制标准D3D绘制流程
	Shader_Begin();

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// 将所有24个顶点数据复制到顶点缓冲区
	memcpy(msr.pData, v, sizeof(Vertex) * 24);

	g_pContext->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	// *** 注意：这里使用 LINELIST ***
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	Texture_Set(g_WhiteId);

	// *** 注意：绘制24个顶点 ***
	g_pContext->Draw(24, 0);
}