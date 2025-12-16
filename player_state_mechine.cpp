#include "player_state_mechine.h"

PlayerStateMachine::PlayerStateMachine()
{
	m_PlayerState = PlayerState::IDLE;
	m_AimState = AimState::HIP;
	m_ActionState = ActionState::NONE;
	m_AccumulatedTime = 0.0f;
	m_AnimationDuration = 0.0f;
}

void PlayerStateMachine::SetPlayerState(PlayerState state)
{
	m_PlayerState = state;
}

PlayerState PlayerStateMachine::GetPlayerState() const
{
	return m_PlayerState;
}

void PlayerStateMachine::SetAimState(AimState state)
{
	m_AimState = state;
}

AimState PlayerStateMachine::GetAimState() const
{
	return m_AimState;
}

void PlayerStateMachine::SetActionState(ActionState state)
{
	m_ActionState = state;
}

ActionState PlayerStateMachine::GetActionState() const
{
	return m_ActionState;
}

void PlayerStateMachine::Update(double elapsed_time)
{
	m_AccumulatedTime += static_cast<float>(elapsed_time);
}

