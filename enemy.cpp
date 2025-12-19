#include "enemy.h"

#include "enemy_normal.h"

void Enemy::Update(double elapsed_time)
{
	if (m_pCurrentState != nullptr)
	{
		m_pCurrentState->Update(elapsed_time);
	}
}

void Enemy::Draw() const
{
	if (m_pCurrentState != nullptr)
	{
		m_pCurrentState->Draw();
	}
}

void Enemy::DepthDraw() const
{
	if (m_pCurrentState != nullptr)
	{
		m_pCurrentState->DepthDraw();
	}
}

void Enemy::UpdateState()
{
	if (m_pNextState != m_pCurrentState)
	{
		delete m_pCurrentState;
		m_pCurrentState = m_pNextState;
	}
}


void Enemy::SetState(State* pNextState)
{
	m_pNextState = pNextState;
}

namespace
{
	constexpr int MAX_ENEMY = 16;
	Enemy* g_pEnemies[MAX_ENEMY]{};
	int g_EnemyCount{ 0 };
}


void Enemy_Initialize()
{
	g_EnemyCount = 0;

}

void Enemy_Finalize()
{
	for (int i = 0; i < g_EnemyCount; i++)
	{
		delete g_pEnemies[i];
	}
}

void Enemy_UpdateAll(double elapsed_time)
{

	for (int i = 0; i < g_EnemyCount; i++)
	{
		g_pEnemies[i]->UpdateState();
	}

	for (int i = 0; i < g_EnemyCount; i++)
	{
		g_pEnemies[i]->Update(elapsed_time);
	}

	for (int i = g_EnemyCount - 1; i >= 0; i--)
	{
		if (g_pEnemies[i]->isDestroyed())
		{
			delete g_pEnemies[i];
			g_pEnemies[i] = g_pEnemies[--g_EnemyCount];
		}
	}
}

void Enemy_DrawAll()
{
	for (int i = 0; i < g_EnemyCount; i++)
	{
		g_pEnemies[i]->Draw();
	}
}

void Enemy_DepthDrawAll()
{
	for (int i = 0; i < g_EnemyCount; i++)
	{
		g_pEnemies[i]->DepthDraw();
	}
}

void Enemy_Create(const XMFLOAT3& position)
{
	g_pEnemies[g_EnemyCount++] = new EnemyNormal(position);
}

int Enemy_GetEnemyCount()
{
	return g_EnemyCount;
}

Enemy* Enemy_GetEnemy(int index)
{
	return g_pEnemies[index];
}

