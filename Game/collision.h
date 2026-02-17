#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXMath.h>

#include "debug_text.h"


struct Sphere
{
	DirectX::XMFLOAT3 center; // 球心坐标
	float radius; // 半径
};

struct Circle
{
	DirectX::XMFLOAT2 center; // 圆心坐标
	float radius; // 半径
};

struct Box
{
	DirectX::XMFLOAT2 center;
	float halfWidth; // 水平半宽
	float halfHeight; // 垂直半宽
};
struct AABB
{
	DirectX::XMFLOAT3 min;
	DirectX::XMFLOAT3 max;

	DirectX::XMFLOAT3 GetCenter() const
	{
		DirectX::XMFLOAT3 center;
		center.x = min.x + (max.x - min.x) * 0.5f;
		center.y = min.y + (max.y - min.y) * 0.5f;
		center.z = min.z + (max.z - min.z) * 0.5f;
		return center;
	}
};
struct Hit
{
	bool isHit;
	DirectX::XMFLOAT3 normal;
};

struct Capsule
{
	DirectX::XMFLOAT3 pointA; // 线段端点A（底部球心）
	DirectX::XMFLOAT3 pointB; // 线段端点B（顶部球心）
	float radius;             // 半径
};

struct Ray
{
	DirectX::XMFLOAT3 origin;    // 光线起点
	DirectX::XMFLOAT3 direction; // 光线方向（应为单位向量）
};

//2D
bool Collision_OverlapCircleCircle(const Circle& a, const Circle& b);
bool Collision_OverlapCircleBox(const Box& a, const Box& b);

//3D
bool Collision_IsOverLapAABB(const AABB& a, const AABB& b);
Hit Collision_IsHitAABB(const AABB& a, const AABB& b);
bool Collision_OverlapSphere(const Sphere& sphere, const DirectX::XMFLOAT3& point);

bool Collision_isHitRayOnSphere(const Ray& ray, const Sphere& sphere, float* outDistance = nullptr);

bool Collision_OverlapSphere(const Sphere& sphereA, const Sphere& sphereB);

// Capsule
bool Collision_OverlapCapsuleCapsule(const Capsule& a, const Capsule& b);
bool Collision_OverlapCapsuleSphere(const Capsule& capsule, const Sphere& sphere);
bool Collision_OverlapCapsuleAABB(const Capsule& capsule, const AABB& aabb);
bool Collision_isHitRayOnCapsule(const Ray& ray, const Capsule& capsule, float* outDistance = nullptr);
Hit Collision_IsHitCapsuleCapsule(const Capsule& a, const Capsule& b);

void Collision_DebugInitialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Collision_DebugFinalize();
void Collision_DebugDraw(const Circle& circle, const DirectX::XMFLOAT4& color = { 1.0f,1.0f,0.0f,1.0f });
void Collision_DebugDraw(const Box& box, const DirectX::XMFLOAT4& color = { 1.0f,1.0f,0.0f,1.0f });

void Collision_DebugDraw(const AABB& aabb, const DirectX::XMFLOAT4& color);
void Collision_DebugDraw(const Capsule& capsule, const DirectX::XMFLOAT4& color = { 0.0f,1.0f,0.0f,1.0f });


#endif // COLLISION_H