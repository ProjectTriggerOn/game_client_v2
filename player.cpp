#include "player.h"
#include "sprite.h"
#include "texture.h"
#include "keyboard.h"
#include "key_logger.h"

using namespace DirectX;

static XMFLOAT2 g_PlayerPosition{}; // プレイヤーの位置
static XMFLOAT2 g_PlayerVelocity{}; // プレイヤーの速度
static int g_PlayerTextureId = -1;



void Player_Initialize(const XMFLOAT2& position)
{
	g_PlayerPosition = position; // プレイヤーの初期位置を設定
	g_PlayerVelocity = { 0.0f, 0.0f }; // プレイヤーの初期速度を設定
	g_PlayerTextureId = Texture_LoadFromFile(L"resource/texture/kokosozai.png"); // プレイヤーのテクスチャを読み込む

}

void Player_Update(double elapsed_time)
{
	XMVECTOR velocity = XMLoadFloat2(&g_PlayerVelocity); // プレイヤーの速度をXMVECTORに変換
	XMVECTOR position = XMLoadFloat2(&g_PlayerPosition); // プレイヤーの位置をXMVECTORに変換
	XMVECTOR direction{};

	if (KeyLogger_IsPressed(KK_W))
	{
		direction += {0.0f, -1.0f}; // 上方向に速度を加える
	}
	if (KeyLogger_IsPressed(KK_S))
	{
		direction += {0.0f, 1.0f}; // 下方向に速度を加える
	}
	if (KeyLogger_IsPressed(KK_A))
	{
		direction += {-1.0f, 0.0f}; // 左方向に速度を加える
	}
	if (KeyLogger_IsPressed(KK_D))
	{
		direction += {1.0f, 0.0f}; // 右方向に速度を加える
	}

	direction = XMVector2Normalize(direction); // 方向ベクトルを正規化

	velocity += direction * 200.0f * static_cast<float>(elapsed_time); // 速度を更新



	position += velocity;

	velocity *= 0.9f;

	XMStoreFloat2(&g_PlayerPosition, position); // 更新された位置を XMFLOAT2 に変換して保存
	XMStoreFloat2(&g_PlayerVelocity, velocity); // 更新された速度を XMFLOAT2 に変換して保存
}

void Player_Draw()
{
	Sprite_Draw(g_PlayerTextureId, g_PlayerPosition.x, g_PlayerPosition.y,
		64.0f, 64.0f, 0,0,32,32); 
}

void Player_Finalize()
{

}
