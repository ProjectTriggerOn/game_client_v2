#include "player.h"

#include "cube.h"
#include "model.h"
#include "key_logger.h"
#include "Light.h"
#include "player_cam_tps.h"
using namespace DirectX;

namespace 
{
	//プレイヤーの位置
	XMFLOAT3 g_PlayerPosition{};
	//プレイヤーの速度
	XMFLOAT3 g_PlayerVelocity{};

	//プレイヤーの前方向
	XMFLOAT3 g_PlayerFront{};

	//プレイヤーモデル
	MODEL* g_PlayerModel = nullptr;

	bool g_isJump = false;

}

void Player_Initialize(
	const XMFLOAT3& positon,
	const XMFLOAT3& front)
{
	g_PlayerPosition = positon;
	g_PlayerVelocity = XMFLOAT3{ 0.0f,0.0f,0.0f };
	XMStoreFloat3(&g_PlayerFront,XMVector3Normalize(XMLoadFloat3(&front)));
	g_PlayerModel = ModelLoad("resource/model/test.fbx", 0.1f);
}

void Player_Finalize()
{
	ModelRelease(g_PlayerModel);
}

void Player_Update(double elapsed_time)
{

	XMVECTOR position = XMLoadFloat3(&g_PlayerPosition);
	XMVECTOR velocity = XMLoadFloat3(&g_PlayerVelocity);

	//移動
	if (KeyLogger_IsTrigger(KK_SPACE) && !g_isJump)
	{
		velocity += {0.0f, 8.0f, 0.0f };
		g_isJump = true;
	}

	//重力
	XMVECTOR gravityDir = { 0.0f,-1.0f };
	velocity += gravityDir * 9.8f * 1.5f * static_cast<float>(elapsed_time);
	position += velocity * static_cast<float>(elapsed_time);
	
	//地面判定
	if (XMVectorGetY(position) < 0.0f)
	{
		position -= velocity * static_cast<float>(elapsed_time);
		velocity *= { 1.0f, 0.0f, 1.0f };

		g_isJump = false;
	}

	XMVECTOR moveDir = XMVectorZero();

	XMFLOAT3 camFront = PlayerCamTps_GetFront();
	XMVECTOR front = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), front));

	if (KeyLogger_IsPressed(KK_W)) moveDir += front;
	if (KeyLogger_IsPressed(KK_S)) moveDir -= front;
	if (KeyLogger_IsPressed(KK_D)) moveDir += right;
	if (KeyLogger_IsPressed(KK_A)) moveDir -= right;

	if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0f) {

		moveDir = XMVector3Normalize(moveDir);

		float dot = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&g_PlayerFront), moveDir));

		float angle = acosf(dot);

		const float ROT_SPEED = XM_2PI * 2.0f * static_cast<float>(elapsed_time);

		if (angle < ROT_SPEED)
		{
			front = moveDir;
		}
		else
		{//回転行列を使って徐々に向きを変える

			XMMATRIX r = XMMatrixIdentity();

			if (XMVectorGetY(XMVector3Cross(XMLoadFloat3(&g_PlayerFront), moveDir)) < 0.0f)
			{
				r = XMMatrixRotationY(-ROT_SPEED);
			}
			else
			{
				r = XMMatrixRotationY(ROT_SPEED);
			}

			front = XMVector3TransformNormal(XMLoadFloat3(&g_PlayerFront), r);
		}

		velocity += front * static_cast<float>(2000.0 / 50.0 * elapsed_time);

		XMStoreFloat3(&g_PlayerFront, front);
	}

	velocity += -velocity* static_cast<float>(4.0f * elapsed_time);
	position += velocity * static_cast<float>(elapsed_time);


	XMStoreFloat3(&g_PlayerPosition, position);
	XMStoreFloat3(&g_PlayerVelocity, velocity);

	//当たり判定実験
	AABB player = Player_GetAABB();
	AABB cube = Cube_GetAABB({ 3.0f, 0.5f, 2.0f });

	if (Collision_IsOverLapAABB(player,cube))
	{
		XMStoreFloat3(&g_PlayerPosition, position - velocity * static_cast<float>(elapsed_time));
		XMStoreFloat3(&g_PlayerVelocity, {0.0f,0.0f,0.0f});
	}

}

XMFLOAT3& Player_GetPosition()
{
	return g_PlayerPosition;
}

XMFLOAT3& Player_GetFront()
{
	return g_PlayerFront;
}

AABB Player_GetAABB()
{
	//y軸はプレイヤーの高さを考慮
	return {
		{g_PlayerPosition.x - 1.0f,  g_PlayerPosition.y, g_PlayerPosition.z - 1.0f },
		{g_PlayerPosition.x + 1.0f,g_PlayerPosition.y + 2.0f, g_PlayerPosition.z + 1.0f}
	};
}

void Player_Draw()
{
	Light_SetSpecularWorld(PlayerCamTps_GetPosition(), 4.0f, { 0.3f,0.25f,0.2f,1.0f });

	float angle = -atan2f(g_PlayerFront.z,g_PlayerFront.x) + XMConvertToRadians(270);

	XMMATRIX r = XMMatrixRotationY(angle);

	XMMATRIX t = XMMatrixTranslation(
		g_PlayerPosition.x,
		g_PlayerPosition.y + 1.0f,
		g_PlayerPosition.z);

	XMMATRIX world = r * t;

	ModelDraw(g_PlayerModel, world);
}