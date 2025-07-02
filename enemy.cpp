#include "enemy.h"
#include <DirectXMath.h>

#include "sprite.h"
#include "texture.h"
using namespace DirectX;

struct EnemyType
{
	int tex_id; // テクスチャID
	int tx, ty, tw, th;
};

struct Enemy
{
	int typeID;
	XMFLOAT2 position; // 敵の位置
	XMFLOAT2 velocity; // 敵の速度
	bool is_enable;
	float offsetY; // 敵のYオフセット
	float offsetX; // 敵のXオフセット
	double lifetime; // 敵の残り時間
};

static constexpr unsigned int MAX_ENEMIES = 1024; // 最大敵数
static constexpr float ENEMY_WIDTH = 64.0f; 
static constexpr float ENEMY_HEIGHT = 64.0f; 

namespace 
{
	Enemy g_Enemies[MAX_ENEMIES]{}; // 敵の配列
	EnemyType g_EnemyTypes[] = {
	{ -1, 0,0,64,64 },
	{-1,0,0,64,64}
	};
}

void Enemy_Initialize()
{
	for (Enemy& e : g_Enemies)
	{
		e.is_enable = false; // 初期化時は全ての敵を無効にする
	}

	g_EnemyTypes[0].tex_id = Texture_LoadFromFile(L"resource/texture/Enemy01.png"); // 敵1のテクスチャID
	g_EnemyTypes[1].tex_id = Texture_LoadFromFile(L"resource/texture/Enemy02.png"); // 敵2のテクスチャID
}

void Enemy_Update(double elapsed_time,double game_time)
{
	for (Enemy& e : g_Enemies)
	{
		if (!e.is_enable) continue;



		switch(e.typeID)
		{
			case ENEMY_GREEN:
				XMVECTOR position = XMLoadFloat2(&e.position);
				XMVECTOR velocity = XMLoadFloat2(&e.velocity);
				position += velocity * static_cast<float>(elapsed_time);
				XMStoreFloat2(& e.position, position);
				XMStoreFloat2(&e.velocity, velocity);
			break;
			case ENEMY_RED:
				e.position.y += e.velocity.y * static_cast<float>(elapsed_time);
				e.position.x = e.offsetX + sin(e.lifetime*3.0) * 450.0f;
				break;
			default:
				break;
		}

		e.lifetime += elapsed_time; // 敵の残り時間を更新

		if (e.position.x + ENEMY_HEIGHT < 0.0f)
		{
			e.is_enable = false;
		}
	}
}

void Enemy_Draw()
{
	for (const Enemy& e : g_Enemies)
	{
		if (!e.is_enable) continue;
		Sprite_Draw(g_EnemyTypes[e.typeID].tex_id, e.position.x, e.position.y,
			ENEMY_WIDTH, ENEMY_HEIGHT,
			g_EnemyTypes[e.typeID].tx, g_EnemyTypes[e.typeID].ty,
			g_EnemyTypes[e.typeID].tw, g_EnemyTypes[e.typeID].th);
	}
}

void Enemy_Finalize()
{
}

void Enemy_Spawn(EnemyTypeID id, const DirectX::XMFLOAT2& position)
{
	for (Enemy& e : g_Enemies)
	{
		if (e.is_enable)continue;

		e.position = position; // 位置設定
		e.velocity = { 0.0f, 100.0f };
		e.is_enable = true;
		e.offsetX = position.x;
		e.typeID = id; // 敵の種類設定
		e.lifetime = 0.0; // 残り時間初期化
		break;

	}
}

