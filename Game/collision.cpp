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

// ============================================================
// 辅助函数：点到线段的最近点（返回参数t in [0,1]）
// ============================================================
static XMVECTOR ClosestPointOnSegment(XMVECTOR segA, XMVECTOR segB, XMVECTOR point, float* outT = nullptr)
{
	XMVECTOR ab = segB - segA;
	float abLenSq = XMVectorGetX(XMVector3LengthSq(ab));
	float t = 0.0f;
	if (abLenSq > 1e-8f) {
		t = XMVectorGetX(XMVector3Dot(point - segA, ab)) / abLenSq;
		t = std::clamp(t, 0.0f, 1.0f);
	}
	if (outT) *outT = t;
	return segA + ab * t;
}

// ============================================================
// 辅助函数：两条线段之间的最近距离的平方及最近点
// ============================================================
static float ClosestDistanceSqSegmentSegment(
	XMVECTOR p1, XMVECTOR q1,
	XMVECTOR p2, XMVECTOR q2,
	XMVECTOR* outClosest1 = nullptr,
	XMVECTOR* outClosest2 = nullptr)
{
	XMVECTOR d1 = q1 - p1; // 线段1方向
	XMVECTOR d2 = q2 - p2; // 线段2方向
	XMVECTOR r = p1 - p2;

	float a = XMVectorGetX(XMVector3Dot(d1, d1)); // |d1|^2
	float e = XMVectorGetX(XMVector3Dot(d2, d2)); // |d2|^2
	float f = XMVectorGetX(XMVector3Dot(d2, r));

	float s, t;
	const float EPSILON = 1e-8f;

	if (a <= EPSILON && e <= EPSILON) {
		// 两条线段都退化为点
		s = t = 0.0f;
	}
	else if (a <= EPSILON) {
		// 线段1退化为点
		s = 0.0f;
		t = std::clamp(f / e, 0.0f, 1.0f);
	}
	else {
		float c = XMVectorGetX(XMVector3Dot(d1, r));
		if (e <= EPSILON) {
			// 线段2退化为点
			t = 0.0f;
			s = std::clamp(-c / a, 0.0f, 1.0f);
		}
		else {
			float b = XMVectorGetX(XMVector3Dot(d1, d2));
			float denom = a * e - b * b;

			if (denom != 0.0f) {
				s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
			}
			else {
				s = 0.0f;
			}

			t = (b * s + f) / e;

			if (t < 0.0f) {
				t = 0.0f;
				s = std::clamp(-c / a, 0.0f, 1.0f);
			}
			else if (t > 1.0f) {
				t = 1.0f;
				s = std::clamp((b - c) / a, 0.0f, 1.0f);
			}
		}
	}

	XMVECTOR closest1 = p1 + d1 * s;
	XMVECTOR closest2 = p2 + d2 * t;
	if (outClosest1) *outClosest1 = closest1;
	if (outClosest2) *outClosest2 = closest2;

	return XMVectorGetX(XMVector3LengthSq(closest1 - closest2));
}

// ============================================================
// Capsule vs Capsule 重叠判定
// ============================================================
bool Collision_OverlapCapsuleCapsule(const Capsule& a, const Capsule& b)
{
	XMVECTOR a1 = XMLoadFloat3(&a.pointA);
	XMVECTOR a2 = XMLoadFloat3(&a.pointB);
	XMVECTOR b1 = XMLoadFloat3(&b.pointA);
	XMVECTOR b2 = XMLoadFloat3(&b.pointB);

	float distSq = ClosestDistanceSqSegmentSegment(a1, a2, b1, b2);
	float radiusSum = a.radius + b.radius;
	return distSq <= radiusSum * radiusSum;
}

// ============================================================
// Capsule vs Capsule 碰撞判定（带法线）
// ============================================================
Hit Collision_IsHitCapsuleCapsule(const Capsule& a, const Capsule& b)
{
	Hit hit{};

	XMVECTOR a1 = XMLoadFloat3(&a.pointA);
	XMVECTOR a2 = XMLoadFloat3(&a.pointB);
	XMVECTOR b1 = XMLoadFloat3(&b.pointA);
	XMVECTOR b2 = XMLoadFloat3(&b.pointB);

	XMVECTOR closestA, closestB;
	float distSq = ClosestDistanceSqSegmentSegment(a1, a2, b1, b2, &closestA, &closestB);
	float radiusSum = a.radius + b.radius;

	hit.isHit = (distSq <= radiusSum * radiusSum);
	if (!hit.isHit) return hit;

	XMVECTOR diff = closestA - closestB;
	float dist = sqrtf(distSq);
	if (dist > 1e-8f) {
		XMVECTOR normal = diff / dist;
		XMStoreFloat3(&hit.normal, normal);
	}
	else {
		hit.normal = { 0.0f, 1.0f, 0.0f }; // 重叠时默认向上
	}
	return hit;
}

// ============================================================
// Capsule vs Sphere 重叠判定
// ============================================================
bool Collision_OverlapCapsuleSphere(const Capsule& capsule, const Sphere& sphere)
{
	XMVECTOR a = XMLoadFloat3(&capsule.pointA);
	XMVECTOR b = XMLoadFloat3(&capsule.pointB);
	XMVECTOR center = XMLoadFloat3(&sphere.center);

	XMVECTOR closest = ClosestPointOnSegment(a, b, center);
	XMVECTOR diff = center - closest;
	float distSq = XMVectorGetX(XMVector3LengthSq(diff));
	float radiusSum = capsule.radius + sphere.radius;
	return distSq <= radiusSum * radiusSum;
}

// ============================================================
// Capsule vs AABB 重叠判定
// ============================================================
bool Collision_OverlapCapsuleAABB(const Capsule& capsule, const AABB& aabb)
{
	// 将胶囊体线段上采样多个点，找到离AABB最近的距离
	// 更精确的做法：找线段上离AABB最近的点
	XMVECTOR segA = XMLoadFloat3(&capsule.pointA);
	XMVECTOR segB = XMLoadFloat3(&capsule.pointB);
	XMVECTOR aabbMin = XMLoadFloat3(&aabb.min);
	XMVECTOR aabbMax = XMLoadFloat3(&aabb.max);

	// 在线段上搜索离AABB最近的点（迭代法）
	// 对线段参数t进行二分/采样
	float bestDistSq = FLT_MAX;
	const int STEPS = 16;
	for (int i = 0; i <= STEPS; ++i) {
		float t = static_cast<float>(i) / STEPS;
		XMVECTOR segPoint = segA + (segB - segA) * t;

		// AABB上离segPoint最近的点（clamp到AABB范围）
		XMVECTOR clamped = XMVectorClamp(segPoint, aabbMin, aabbMax);
		float distSq = XMVectorGetX(XMVector3LengthSq(segPoint - clamped));
		if (distSq < bestDistSq) {
			bestDistSq = distSq;
		}
	}

	return bestDistSq <= capsule.radius * capsule.radius;
}

// ============================================================
// Ray vs Capsule 射线判定
// ============================================================
bool Collision_isHitRayOnCapsule(const Ray& ray, const Capsule& capsule, float* outDistance)
{
	// 将胶囊体视为：无限圆柱体 + 两端半球
	// 先检测射线与圆柱体部分，再检测与两端球

	XMVECTOR rayOrig = XMLoadFloat3(&ray.origin);
	XMVECTOR rayDir = XMLoadFloat3(&ray.direction);
	XMVECTOR capA = XMLoadFloat3(&capsule.pointA);
	XMVECTOR capB = XMLoadFloat3(&capsule.pointB);
	XMVECTOR capAxis = capB - capA;
	float capLenSq = XMVectorGetX(XMVector3LengthSq(capAxis));

	float bestT = FLT_MAX;
	bool hasHit = false;

	if (capLenSq > 1e-8f) {
		// 圆柱体部分的射线检测
		XMVECTOR capDir = capAxis / sqrtf(capLenSq);
		XMVECTOR oc = rayOrig - capA;

		// 去除沿胶囊体轴方向的分量
		float dDotAxis = XMVectorGetX(XMVector3Dot(rayDir, capDir));
		float ocDotAxis = XMVectorGetX(XMVector3Dot(oc, capDir));

		XMVECTOR dPerp = rayDir - capDir * dDotAxis;
		XMVECTOR ocPerp = oc - capDir * ocDotAxis;

		float a = XMVectorGetX(XMVector3Dot(dPerp, dPerp));
		float b = XMVectorGetX(XMVector3Dot(dPerp, ocPerp));
		float c = XMVectorGetX(XMVector3Dot(ocPerp, ocPerp)) - capsule.radius * capsule.radius;

		float disc = b * b - a * c;
		if (disc >= 0.0f && a > 1e-8f) {
			float sqrtDisc = sqrtf(disc);
			float t = (-b - sqrtDisc) / a;
			if (t < 0.0f) t = (-b + sqrtDisc) / a;

			if (t >= 0.0f) {
				// 检查交点是否在胶囊体圆柱段内
				float hitOnAxis = ocDotAxis + t * dDotAxis;
				float capLen = sqrtf(capLenSq);
				if (hitOnAxis >= 0.0f && hitOnAxis <= capLen) {
					if (t < bestT) { bestT = t; hasHit = true; }
				}
			}
		}
	}

	// 检测两端半球
	Sphere sphereA = { capsule.pointA, capsule.radius };
	Sphere sphereB = { capsule.pointB, capsule.radius };
	float tA, tB;
	if (Collision_isHitRayOnSphere(ray, sphereA, &tA)) {
		if (tA < bestT) { bestT = tA; hasHit = true; }
	}
	if (Collision_isHitRayOnSphere(ray, sphereB, &tB)) {
		if (tB < bestT) { bestT = tB; hasHit = true; }
	}

	if (hasHit && outDistance) {
		*outDistance = bestT;
	}
	return hasHit;
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

void Collision_DebugDraw(const Capsule& capsule, const DirectX::XMFLOAT4& color)
{
	const int CIRCLE_SEGMENTS = 16; // 每个圆环的分段数
	const int NUM_RINGS = 3;        // 沿轴方向的圆环数（除两端）

	XMVECTOR a = XMLoadFloat3(&capsule.pointA);
	XMVECTOR b = XMLoadFloat3(&capsule.pointB);
	XMVECTOR axis = b - a;
	float axisLen = XMVectorGetX(XMVector3Length(axis));

	// 构建正交基
	XMVECTOR up;
	if (axisLen > 1e-6f) {
		up = XMVector3Normalize(axis);
	}
	else {
		up = XMVectorSet(0, 1, 0, 0);
	}

	// 找一个不平行的向量来叉乘
	XMVECTOR helper = XMVectorSet(1, 0, 0, 0);
	if (fabsf(XMVectorGetX(XMVector3Dot(up, helper))) > 0.9f) {
		helper = XMVectorSet(0, 0, 1, 0);
	}
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, helper));
	XMVECTOR forward = XMVector3Cross(right, up);

	// 顶点数组
	// 圆环线: (NUM_RINGS + 2) * CIRCLE_SEGMENTS * 2 (LINELIST)
	// 连接线: 4条纵线 * 2 顶点 = 8
	// 半球弧线: 两端各2条弧(right平面和forward平面) * (CIRCLE_SEGMENTS/2+1) 顶点
	const int ringVertices = (NUM_RINGS + 2) * CIRCLE_SEGMENTS * 2;
	const int lineVertices = 8;
	const int arcSegments = CIRCLE_SEGMENTS / 2;
	const int arcVertices = 4 * arcSegments * 2; // 4条弧线
	const int totalVertices = ringVertices + lineVertices + arcVertices;

	Vertex v[NUM_VERTEX]; // 使用已有的上限
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

	// 绘制圆环（底端、顶端、中间）
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

	// 绘制4条纵线连接底端和顶端
	for (int i = 0; i < 4; ++i) {
		float angle = XM_2PI * i / 4;
		XMVECTOR offset = (right * cosf(angle) + forward * sinf(angle)) * capsule.radius;
		addLine(a + offset, b + offset);
	}

	// 绘制两端半球弧线
	// 底端半球（朝下）: right平面弧和forward平面弧
	for (int plane = 0; plane < 2; ++plane) {
		XMVECTOR planeDir = (plane == 0) ? right : forward;
		for (int i = 0; i < arcSegments; ++i) {
			float angle0 = XM_PI * 0.5f + XM_PI * i / arcSegments;
			float angle1 = XM_PI * 0.5f + XM_PI * (i + 1) / arcSegments;

			XMVECTOR p0 = a + planeDir * (cosf(angle0) * capsule.radius) + up * (sinf(angle0) * capsule.radius);
			XMVECTOR p1 = a + planeDir * (cosf(angle1) * capsule.radius) + up * (sinf(angle1) * capsule.radius);
			addLine(p0, p1);
		}
	}

	// 顶端半球（朝上）
	for (int plane = 0; plane < 2; ++plane) {
		XMVECTOR planeDir = (plane == 0) ? right : forward;
		for (int i = 0; i < arcSegments; ++i) {
			float angle0 = -XM_PI * 0.5f + XM_PI * i / arcSegments;
			float angle1 = -XM_PI * 0.5f + XM_PI * (i + 1) / arcSegments;

			XMVECTOR p0 = b + planeDir * (cosf(angle0) * capsule.radius) + up * (sinf(angle0) * capsule.radius);
			XMVECTOR p1 = b + planeDir * (cosf(angle1) * capsule.radius) + up * (sinf(angle1) * capsule.radius);
			addLine(p0, p1);
		}
	}

	// 绘制
	Shader_Begin();

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