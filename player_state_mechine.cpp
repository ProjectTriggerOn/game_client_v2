#include "player_state_mechine.h"

PlayerStateMachine::PlayerStateMachine()
{
	m_PlayerState = PlayerState::IDLE;
	m_WeaponState = WeaponState::TAKING_OUT;
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

void PlayerStateMachine::SetWeaponState(WeaponState state)
{
	m_WeaponState = state;
}

WeaponState PlayerStateMachine::GetWeaponState() const
{
	return m_WeaponState;
}

void PlayerStateMachine::Update(double elapsed_time, Animator* animator)
{
	m_AccumulatedTime += static_cast<float>(elapsed_time);

	int animationIndex = -1;

	if (m_WeaponState == WeaponState::ADS_IN &&
		animator->GetCurrentAnimationIndex() == 4 &&
		animator->GetCurrAniProgress() > 0.3f)
	{
		m_WeaponState = WeaponState::ADS;
	}

	if (m_WeaponState == WeaponState::ADS_OUT &&
		animator->GetCurrentAnimationIndex() == 5 &&
		animator->GetCurrAniProgress() > 0.3f)
	{
		m_WeaponState = WeaponState::HIP;
	}

	if (m_WeaponState == WeaponState::RELOADING &&
		animator->GetCurrentAnimationIndex() == 9 &&
		animator->GetCurrAniProgress() > 0.9f)
	{
		m_WeaponState = WeaponState::HIP;
	}

	if (m_WeaponState == WeaponState::TAKING_OUT &&
		animator->GetCurrentAnimationIndex() == 12 &&
		animator->GetCurrAniProgress() > 0.8f)
	{
		m_WeaponState = WeaponState::HIP;
	}

	if (m_WeaponState == WeaponState::HIP_FIRING &&
		animator->GetCurrentAnimationIndex() == 3 &&
		animator->GetCurrAniProgress() > 0.70f &&
		animator->IsBlending() == false)
	{
		//animator->SetSameAniOverlapAllow(false);
		m_WeaponState = WeaponState::HIP;
	}


	switch (m_PlayerState)
	{
	case PlayerState::IDLE:
		animationIndex = 0;
		break;
	case PlayerState::WALKING:
		animationIndex = 1;
		break;
	case PlayerState::RUNNING:
		animationIndex = 2;
		break;
	}
	switch (m_WeaponState)
	{

	case WeaponState::HIP:
		// No change to animationIndex
		break;
	case WeaponState::HIP_FIRING:
		animationIndex = 3;
		break;
	case WeaponState::ADS_IN:
		animationIndex = 4;
		break;
	case WeaponState::ADS_OUT:
		animationIndex = 5;
		break;
	case WeaponState::ADS:
		animationIndex = 6;
		break;
	case WeaponState::ADS_FIRING:
		animationIndex = 7;
		break;
	case WeaponState::RELOADING:
		animationIndex = 9;
		break;
	case WeaponState::RELOADING_OUT_OF_AMMO:
		animationIndex = 10;
		break;
	case WeaponState::INSPECTING:
		animationIndex = 11;
		break;
	case WeaponState::TAKING_OUT:
		animationIndex = 12;
		break;
	}

	bool isLoopAnimation{true};
	float blendTime = 0.1f;
	switch (animationIndex)
	{
	case 0: // IDLE
	case 1: // WALKING

	case 2: // RUNNING
	case 7: // ADS_FIRING
	case 11: // INSPECTING
	case 6: // ADS
		isLoopAnimation = true;
			break;

	case 4: // ADS_IN
	case 3: // HIP_FIRING
	case 5: // ADS_OUT
	case 9: // RELOADING
	case 10: // RELOADING_OUT_OF_AMMO
	case 12: // TAKING_OUT
		isLoopAnimation = false;
			break;
	default:
		break;
	}

	//if (animator->GetCurrentAnimationIndex() != animationIndex)
	//{
		animator->PlayCrossFade(animationIndex, isLoopAnimation, 0.1f);
	//}else(animationIndex )
	animator->Update(elapsed_time);
}

