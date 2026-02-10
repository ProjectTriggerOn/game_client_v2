#pragma once
#include <string>
#include <unordered_map>

#include "animator.h"

//=============================================================================
// Player State / Weapon State Enums
//=============================================================================
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

//=============================================================================
// ToString Helpers
//=============================================================================
const char* PlayerStateToString(PlayerState state);
const char* WeaponStateToString(WeaponState state);

//=============================================================================
// AnimConfig — Maps a state to its animation index, loop flag, blend time
//=============================================================================
struct AnimConfig
{
	int   animIndex;
	bool  loop;
	float blendTime;
};

//=============================================================================
// AutoTransition — When a one-shot animation reaches a progress threshold,
//                  the weapon state automatically transitions to nextState.
//=============================================================================
struct AutoTransition
{
	float       progressThreshold;
	WeaponState nextState;
	bool        requireNotBlending;  // extra condition: only transition when not blending
};

//=============================================================================
// PlayerStateMachine
//=============================================================================
class PlayerStateMachine
{

private:
	PlayerState m_PlayerState;
	WeaponState m_WeaponState;

	//---- Data Tables (populated once in constructor) ----
	std::unordered_map<PlayerState, AnimConfig>     m_PlayerAnimTable;
	std::unordered_map<WeaponState, AnimConfig>     m_WeaponAnimTable;
	std::unordered_map<WeaponState, AutoTransition> m_AutoTransitions;

	void BuildTables();

public:
	PlayerStateMachine();
	~PlayerStateMachine() = default;

	void SetPlayerState(PlayerState state);
	PlayerState GetPlayerState() const;
	void SetWeaponState(WeaponState state);
	WeaponState GetWeaponState() const;

	void Update(double elapsed_time, Animator* animator);
};