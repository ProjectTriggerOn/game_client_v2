#pragma once
#include "enemy.h"
#include <DirectXMath.h>

#include "texture.h"
using namespace DirectX;

class EnemyNormal : public Enemy
{

private:
	XMFLOAT3 m_position{};
	float m_DetectionRadius{ 7.0f };
	int m_Hp{ 100 };
	int m_WhiteTexId{ -1 };
	int m_RedTexId{ -1 };

public:
	EnemyNormal(const XMFLOAT3& position)
		: m_position(position)
	{
		m_WhiteTexId = Texture_LoadFromFile(L"resource/texture/white.png");
		m_RedTexId = Texture_LoadFromFile(L"resource/texture/red.png");
		SetState(new EnemyNormalState_Patrol(this));
	}

	void Damage(int damage) override
	{
		m_Hp -= damage;
	}

	bool isDestroyed() const override
	{
		return m_Hp <= 0;
	}

	Sphere GetCollisionSphere() const override
	{
		return { m_position,0.5f };
	}

private:
	class EnemyNormalState_Patrol : public State
	{
	private:
		EnemyNormal* m_pStateHolder{};
		XMFLOAT3 m_PatrolPointA{};
		double g_AccumulatedTime{};
	public:
		EnemyNormalState_Patrol(EnemyNormal* pHolder)
			: m_pStateHolder(pHolder),
			m_PatrolPointA{ pHolder->m_position }
		{

		}
		void Update(double elapsed_time) override;
		void Draw() const override;
		void DepthDraw() const override;

	};

	class EnemyNormalState_Chase : public State
	{
	private:
		EnemyNormal* m_pStateHolder{};
		double g_AccumulatedTime{};
		int m_TexId{ -1 };
	public:
		EnemyNormalState_Chase(EnemyNormal* pHolder)
			: m_pStateHolder(pHolder)
		{

		}
		void Update(double elapsed_time) override;
		void Draw() const override;
		void DepthDraw() const override;
	};
};

