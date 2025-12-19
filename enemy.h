#pragma once

#include <DirectXMath.h>

#include "collision.h"
using namespace DirectX;

class Enemy
{
protected:
	class State
	{
	public:
		virtual ~State() = default;
		virtual void Update(double elapsed_time) = 0;
		virtual void Draw() const = 0;
		virtual void DepthDraw() const = 0;
	};
private:
	State* m_pCurrentState = nullptr;
	State* m_pNextState = nullptr;

public:
	virtual ~Enemy() = default;
	virtual void Update(double elapsed_time);
	virtual void Draw() const;
	virtual void DepthDraw() const;
	void UpdateState();
	virtual void Damage(int damage) = 0;
	virtual bool isDestroyed() const = 0;
	virtual Sphere GetCollisionSphere() const
	{
		return {};
	};

protected:
	void SetState(State* pNextState);
};

void Enemy_Initialize();
void Enemy_Finalize();
void Enemy_UpdateAll(double elapsed_time);
void Enemy_DrawAll();
void Enemy_DepthDrawAll();
void Enemy_Create(const XMFLOAT3& position);
int Enemy_GetEnemyCount();
Enemy* Enemy_GetEnemy(int index);

