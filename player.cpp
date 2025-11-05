#include "player.h"
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

	XMVECTOR direction{};
	XMVECTOR front = XMLoadFloat3(&PlayerCamTps_GetFront()) * XMVECTOR { 1.0f, 0.0f, 1.0f };

	if (KeyLogger_IsPressed(KK_W))
	{
		direction += front;
	}
	if (KeyLogger_IsPressed(KK_S))
	{
		direction -= front;
	}


	if (KeyLogger_IsPressed(KK_A))
	{
		direction -= XMVector3Cross({ 0.0f,1.0f,0.0f }, front);

	}
	if (KeyLogger_IsPressed(KK_D))
	{
		direction += XMVector3Cross({ 0.0f,1.0f,0.0f }, front);
	}

	if (XMVectorGetX(XMVector3LengthSq(direction)) > 0.0f) {

		direction = XMVector3Normalize(direction);


		float dot = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&g_PlayerFront), direction));

		float angle = acosf(dot);

		const float ROT_SPEED = XM_2PI * 2.0f * static_cast<float>(elapsed_time);

		if (angle < ROT_SPEED)
		{
			front = direction;
		}
		else
		{//回転行列を使って徐々に向きを変える

			XMMATRIX r = XMMatrixIdentity();

			if (XMVectorGetY(XMVector3Cross(XMLoadFloat3(&g_PlayerFront), direction)) < 0.0f)
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

}

XMFLOAT3& Player_GetPosition()
{
	return g_PlayerPosition;
}

XMFLOAT3& Player_GetFront()
{
	return g_PlayerFront;
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