#include "bullet.h"

#include "direct3d.h"
#include "sprite.h"
#include "texture.h"
#include "sprite.h"
using namespace DirectX;
struct Bullet
{
	XMFLOAT2 position; // 弾の位置
	XMFLOAT2 velocity; // 弾の速度
	bool is_enable;
	double lifetime; // 弾の残り時間

};



static constexpr unsigned int MAX_BULLETS = 1024; // 最大弾数
static Bullet g_Bullets[MAX_BULLETS]{}; // 弾の配列

static int g_BulletId = -1;

void Bullet_Initialize(const XMFLOAT2& position)
{
	for (Bullet& b : g_Bullets)
	{
		b.is_enable = false; // 初期化時は全ての弾を無効にする

	}

	g_BulletId = Texture_LoadFromFile(L"resource/texture/laser.png"); // 弾のテクスチャID
}

void Bullet_Update(double elapsed_time)
{
	for (Bullet& b : g_Bullets)
	{
		if (!b.is_enable)continue;

		XMVECTOR position = XMLoadFloat2(&b.position); // 弾の位置をXMVECTORに変換
		XMVECTOR velocity = XMLoadFloat2(&b.velocity); // 弾の速度をXMVECTORに変換

		position += velocity * elapsed_time;

		XMStoreFloat2(&b.position, position); // 更新された位置を XMFLOAT2 に変換して保存
		XMStoreFloat2(&b.velocity, velocity); // 更新された速度を XMFLOAT2 に変換して保存

		b.lifetime += elapsed_time;

		if (b.lifetime > 5.0) // 5秒経過したら弾を無効化
		{
			b.is_enable = false;
		}

		if (b.position.x > Direct3D_GetBackBufferWidth())
		{
			b.is_enable = false;
		}

	}

}

void Bullet_Draw()
{
	for (Bullet& b : g_Bullets)
	{
		if (!b.is_enable) continue;
		Sprite_Draw(g_BulletId, b.position.x, b.position.y,
			9.0f, 27.0f, 0, 0, 9, 27);
	}
}

void Bullet_Finalize()
{
}

void Bullet_Spawn(const DirectX::XMFLOAT2& position)
{
	for (Bullet& b : g_Bullets)
	{
		if (b.is_enable) continue;
		//空き領域発見
		
		b.position = position; // 弾の位置を設定
		b.velocity = { 0.0f, -1000.0f }; // 弾の速度を設定
		b.is_enable = true; // 弾を有効化
		b.lifetime = 0.0; // 残り時間をリセット
		break;
	}
}
