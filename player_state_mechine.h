#pragma once
#include <string>

#include "animator.h"

enum class PlayerState
{
	IDLE,
	WALKING,
	RUNNING,
};

enum class WeaponState
{
	HIP,
	HIP_FIRING,
	ADS_FIRING,
	ADS_IN,
	ADS_OUT,
	ADS,
	RELOADING,
	RELOADING_OUT_OF_AMMO,
	INSPECTING,
	TAKING_OUT,
};

class PlayerStateMachine
{

private:
	PlayerState m_PlayerState;
	WeaponState m_WeaponState;
	float m_AccumulatedTime;
	float m_AnimationDuration;
	std::string m_CurrentAnimation;
public:
	PlayerStateMachine();
	~PlayerStateMachine() = default;
	void SetPlayerState(PlayerState state);
	PlayerState GetPlayerState() const;
	void SetWeaponState(WeaponState state);
	WeaponState GetWeaponState() const;

	void Update(double elapsed_time, Animator* animator);
};