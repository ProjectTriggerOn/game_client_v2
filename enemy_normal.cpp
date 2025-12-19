#include "enemy_normal.h"

#include "collision.h"
#include "cube.h"
#include "Light.h"
#include "player.h"
#include "player_cam_tps.h"

void EnemyNormal::EnemyNormalState_Patrol::Update(double elapsed_time)
{
	g_AccumulatedTime += elapsed_time;
	m_pStateHolder->m_position.x = m_PatrolPointA.x + sinf(static_cast<float>(g_AccumulatedTime)) * 5.0f;

	if (Collision_OverlapSphere(
		{ m_pStateHolder->m_position, m_pStateHolder->m_DetectionRadius }, Player_GetPosition()))
	{
		m_pStateHolder->SetState(
			new EnemyNormalState_Chase(m_pStateHolder)
		);
	}

}

void EnemyNormal::EnemyNormalState_Patrol::Draw() const
{
	Light_SetSpecularWorld(PlayerCamTps_GetPosition(), 4.0f, { 0.3f,0.3f,0.3f,1.0f });

	Cube_Draw(m_pStateHolder->m_WhiteTexId, XMMatrixTranslation(
		m_pStateHolder->m_position.x,
		m_pStateHolder->m_position.y,
		m_pStateHolder->m_position.z
	));
}

void EnemyNormal::EnemyNormalState_Patrol::DepthDraw() const
{

}

void EnemyNormal::EnemyNormalState_Chase::Update(double elapsed_time)
{


	//プレイヤーはこっちだー
	XMVECTOR toPlayer = XMLoadFloat3(&Player_GetPosition()) - XMLoadFloat3(&m_pStateHolder->m_position);
	toPlayer = XMVector3Normalize(toPlayer);

	//歩くぜー
	XMVECTOR position = XMLoadFloat3(&m_pStateHolder->m_position) + toPlayer * 2.0f * static_cast<float>(elapsed_time);
	XMStoreFloat3(&m_pStateHolder->m_position, position);

	//諦める
	//もしプレイヤーが検出範囲外なら、3秒後にパトロール状態へ戻る
	if (!Collision_OverlapSphere(
		{ m_pStateHolder->m_position, m_pStateHolder->m_DetectionRadius }, Player_GetPosition()))
	{
		g_AccumulatedTime += elapsed_time;

		if (g_AccumulatedTime >= 3.0) {

			m_pStateHolder->SetState(
				new EnemyNormalState_Patrol(m_pStateHolder)
			);
		}
	}

}

void EnemyNormal::EnemyNormalState_Chase::Draw() const
{
	Cube_Draw(m_pStateHolder->m_RedTexId, XMMatrixTranslation(
		m_pStateHolder->m_position.x,
		m_pStateHolder->m_position.y,
		m_pStateHolder->m_position.z
	));
}

void EnemyNormal::EnemyNormalState_Chase::DepthDraw() const
{
	Cube_Draw(m_pStateHolder->m_RedTexId, XMMatrixTranslation(
		m_pStateHolder->m_position.x,
		m_pStateHolder->m_position.y,
		m_pStateHolder->m_position.z
	));
}
