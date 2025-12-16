#pragma once
#include <string>

enum class PlayerState
{
	IDLE,
	WALKING,
	RUNNING,
};

enum class AimState
{
	HIP,
	ADS_IN,
	ADS_OUT,
	ADS_IDLE
};

enum class ActionState
{
	NONE,
	FIRING,
	RELOADING,
	INSPECTING,
	TAKING_OUT,
};

class PlayerStateMachine
{

private:
	PlayerState m_PlayerState;
	AimState m_AimState;
	ActionState m_ActionState;
	float m_AccumulatedTime;
	float m_AnimationDuration;
	std::string m_CurrentAnimation;
public:
	PlayerStateMachine();
	~PlayerStateMachine() = default;
	void SetPlayerState(PlayerState state);
	PlayerState GetPlayerState() const;
	void SetAimState(AimState state);
	AimState GetAimState() const;
	void SetActionState(ActionState state);
	ActionState GetActionState() const;

	void Update(double elapsed_time);
};