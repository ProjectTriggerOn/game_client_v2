#include "player.h"
#include "sprite.h"
#include "texture.h"
#include "keyboard.h"
#include "key_logger.h"
#include "bullet.h"
#include "direct3d.h"

using namespace DirectX;

namespace{
	XMFLOAT2 g_PlayerPosition{}; // プレイヤーの位置
	XMFLOAT2 g_PlayerVelocity{}; // プレイヤーの速度
	int g_PlayerTextureId = -1;
	Circle g_PlayerCollision{ {32,32}, 32.0f }; // プレイヤーの衝突判定用の円
	bool g_PlayerIsEnable = true; // プレイヤーが有効かどうか
}

void Player_Initialize(const XMFLOAT2& position)
{
	g_PlayerPosition = position; // プレイヤーの初期位置を設定
	g_PlayerVelocity = { 0.0f, 0.0f }; // プレイヤーの初期速度を設定
	g_PlayerTextureId = Texture_LoadFromFile(L"resource/texture/p1.png"); // プレイヤーのテクスチャを読み込む
}

void Player_Update(double elapsed_time)
{
	if (!g_PlayerIsEnable) return; // プレイヤーが無効な場合は更新しない
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

	velocity += direction * 6000000.0f/50000.0f * static_cast<float>(elapsed_time); // 速度を更新

	position += velocity;

	velocity += -velocity * 4.0f * elapsed_time;

	XMStoreFloat2(&g_PlayerPosition, position); // 更新された位置を XMFLOAT2 に変換して保存
	XMStoreFloat2(&g_PlayerVelocity, velocity); // 更新された速度を XMFLOAT2 に変換して保存


	g_PlayerPosition.x = std::max(0.0f+370.f, g_PlayerPosition.x);

	g_PlayerPosition.x = std::min(Direct3D_GetBackBufferWidth() - 64.0f-370.0f, g_PlayerPosition.x);

	g_PlayerPosition.y = std::max(0.0f, g_PlayerPosition.y);

	g_PlayerPosition.y = std::min(Direct3D_GetBackBufferHeight() - 64.0f, g_PlayerPosition.y);

	if (KeyLogger_IsTrigger(KK_SPACE))
	{
		Bullet_Spawn({g_PlayerPosition.x+30,g_PlayerPosition.y}); 
	}
}

void Player_Draw()
{
	if (!g_PlayerIsEnable) return; // プレイヤーが無効な場合は更新しない
	Sprite_Draw(g_PlayerTextureId, g_PlayerPosition.x, g_PlayerPosition.y,
		64.0f, 64.0f, 0,0,64,64); 
}

void Player_Finalize()
{

}

bool Player_IsEnable()
{
	return g_PlayerIsEnable;
}

Circle Player_GetCollision()
{
	float centerX = g_PlayerPosition.x + g_PlayerCollision.center.x;
	float centerY = g_PlayerPosition.y + g_PlayerCollision.center.y;
	return { {centerX, centerY}, g_PlayerCollision.radius };
}

void Player_Destroy()
{
	g_PlayerIsEnable = false; // プレイヤーを無効化
	//Texture_Release(g_PlayerTextureId); // プレイヤーのテクスチャを解放
}
