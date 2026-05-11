#include "collision.h"

#include <d3d11.h>
#include <DirectXMath.h>

#include "debug_ostream.h"
#include "direct3d.h"
#include "shader.h"
#include "texture.h"
#include <algorithm>
using namespace DirectX;


static constexpr int NUM_VERTEX = 5000; // 頂点数の上限

namespace
{
	ID3D11Buffer* g_pVertexBuffer = nullptr; // 頂点バッファ
	ID3D11Device* g_pDevice = nullptr;
	ID3D11DeviceContext* g_pContext = nullptr;
	int g_WhiteId = -1; // 白色のテクスチャID
	XMMATRIX g_DebugViewProj = XMMatrixIdentity();
}

void Collision_DebugSetViewProj(const DirectX::XMMATRIX& viewProj)
{
	g_DebugViewProj = viewProj;
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
	// 1. Compute the 8 corners of the AABB
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

	// 2. Define 12 edges (24 vertices)
	Vertex v[24];

	auto set_line = [&](int v_index, int corner_a, int corner_b) {
		v[v_index].position = corners[corner_a];
		v[v_index].color = color;
		v[v_index].uv = { 0.0f, 0.0f };

		v[v_index + 1].position = corners[corner_b];
		v[v_index + 1].color = color;
		v[v_index + 1].uv = { 0.0f, 0.0f };
		};

	// Bottom face (4 edges)
	set_line(0, 0, 1);
	set_line(2, 1, 5);
	set_line(4, 5, 4);
	set_line(6, 4, 0);

	// Top face (4 edges)
	set_line(8, 3, 2);
	set_line(10, 2, 6);
	set_line(12, 6, 7);
	set_line(14, 7, 3);

	// Vertical edges (4 edges)
	set_line(16, 0, 3);
	set_line(18, 1, 2);
	set_line(20, 5, 6);
	set_line(22, 4, 7);


	// 3. Standard D3D draw flow
	Shader_Begin();
	Shader_SetMatrix(g_DebugViewProj);

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

	// Upload all 24 vertices into the vertex buffer
	memcpy(msr.pData, v, sizeof(Vertex) * 24);

	g_pContext->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());

	// LINELIST topology for wireframe edges
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	Texture_Set(g_WhiteId);

	g_pContext->Draw(24, 0);
}

void Collision_DebugDrawLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color)
{
	Vertex v[2];
	v[0].position = start;
	v[0].color = color;
	v[0].uv = { 0, 0 };
	v[1].position = end;
	v[1].color = color;
	v[1].uv = { 0, 0 };

	Shader_Begin();
	Shader_SetMatrix(g_DebugViewProj);

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	memcpy(msr.pData, v, sizeof(Vertex) * 2);
	g_pContext->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Texture_Set(g_WhiteId);

	g_pContext->Draw(2, 0);
}

void Collision_DebugDraw(const Capsule& capsule, const DirectX::XMFLOAT4& color)
{
	const int CIRCLE_SEGMENTS = 16; // segments per ring
	const int NUM_RINGS = 3;        // rings along the axis (excluding both ends)

	XMVECTOR a = XMLoadFloat3(&capsule.pointA);
	XMVECTOR b = XMLoadFloat3(&capsule.pointB);
	XMVECTOR axis = b - a;
	float axisLen = XMVectorGetX(XMVector3Length(axis));

	// Build an orthonormal basis around the capsule axis
	XMVECTOR up;
	if (axisLen > 1e-6f) {
		up = XMVector3Normalize(axis);
	}
	else {
		up = XMVectorSet(0, 1, 0, 0);
	}

	// Pick a non-parallel helper vector for the cross product
	XMVECTOR helper = XMVectorSet(1, 0, 0, 0);
	if (fabsf(XMVectorGetX(XMVector3Dot(up, helper))) > 0.9f) {
		helper = XMVectorSet(0, 0, 1, 0);
	}
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, helper));
	XMVECTOR forward = XMVector3Cross(right, up);

	// Vertex layout (LINELIST):
	//   ring lines:        (NUM_RINGS + 2) * CIRCLE_SEGMENTS * 2
	//   axial connectors:  4 lines * 2 vertices = 8
	//   hemisphere arcs:   4 arcs * arcSegments * 2 vertices
	const int ringVertices = (NUM_RINGS + 2) * CIRCLE_SEGMENTS * 2;
	const int lineVertices = 8;
	const int arcSegments = CIRCLE_SEGMENTS / 2;
	const int arcVertices = 4 * arcSegments * 2;
	const int totalVertices = ringVertices + lineVertices + arcVertices;

	Vertex v[NUM_VERTEX];
	int vi = 0;

	auto addLine = [&](XMVECTOR p1, XMVECTOR p2) {
		if (vi + 2 > NUM_VERTEX) return;
		XMStoreFloat3(&v[vi].position, p1);
		v[vi].color = color;
		v[vi].uv = { 0, 0 };
		vi++;
		XMStoreFloat3(&v[vi].position, p2);
		v[vi].color = color;
		v[vi].uv = { 0, 0 };
		vi++;
	};

	// Draw rings (bottom, top, and intermediate)
	for (int ring = 0; ring <= NUM_RINGS + 1; ++ring) {
		float t = static_cast<float>(ring) / (NUM_RINGS + 1);
		XMVECTOR center = a + axis * t;

		for (int i = 0; i < CIRCLE_SEGMENTS; ++i) {
			float angle0 = XM_2PI * i / CIRCLE_SEGMENTS;
			float angle1 = XM_2PI * (i + 1) / CIRCLE_SEGMENTS;

			XMVECTOR p0 = center + (right * cosf(angle0) + forward * sinf(angle0)) * capsule.radius;
			XMVECTOR p1 = center + (right * cosf(angle1) + forward * sinf(angle1)) * capsule.radius;
			addLine(p0, p1);
		}
	}

	// Draw 4 axial lines connecting bottom and top
	for (int i = 0; i < 4; ++i) {
		float angle = XM_2PI * i / 4;
		XMVECTOR offset = (right * cosf(angle) + forward * sinf(angle)) * capsule.radius;
		addLine(a + offset, b + offset);
	}

	// Draw hemisphere arcs at both ends.
	// Bottom hemisphere (downward): equator -> bottom pole -> equator (PI -> 2*PI)
	for (int plane = 0; plane < 2; ++plane) {
		XMVECTOR planeDir = (plane == 0) ? right : forward;
		for (int i = 0; i < arcSegments; ++i) {
			float angle0 = XM_PI + XM_PI * i / arcSegments;
			float angle1 = XM_PI + XM_PI * (i + 1) / arcSegments;

			XMVECTOR p0 = a + planeDir * (cosf(angle0) * capsule.radius) + up * (sinf(angle0) * capsule.radius);
			XMVECTOR p1 = a + planeDir * (cosf(angle1) * capsule.radius) + up * (sinf(angle1) * capsule.radius);
			addLine(p0, p1);
		}
	}

	// Top hemisphere (upward): equator -> top pole -> equator (0 -> PI)
	for (int plane = 0; plane < 2; ++plane) {
		XMVECTOR planeDir = (plane == 0) ? right : forward;
		for (int i = 0; i < arcSegments; ++i) {
			float angle0 = XM_PI * i / arcSegments;
			float angle1 = XM_PI * (i + 1) / arcSegments;

			XMVECTOR p0 = b + planeDir * (cosf(angle0) * capsule.radius) + up * (sinf(angle0) * capsule.radius);
			XMVECTOR p1 = b + planeDir * (cosf(angle1) * capsule.radius) + up * (sinf(angle1) * capsule.radius);
			addLine(p0, p1);
		}
	}

	// Submit the draw
	Shader_Begin();
	Shader_SetMatrix(g_DebugViewProj);

	D3D11_MAPPED_SUBRESOURCE msr;
	g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	memcpy(msr.pData, v, sizeof(Vertex) * vi);
	g_pContext->Unmap(g_pVertexBuffer, 0);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);

	Shader_SetWorldMatrix(DirectX::XMMatrixIdentity());
	g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Texture_Set(g_WhiteId);

	g_pContext->Draw(vi, 0);
}