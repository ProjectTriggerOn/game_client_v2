#include "player_state_mechine.h"

//=============================================================================
// ToString Helpers
//=============================================================================
const char* PlayerStateToString(PlayerState state)
{
	switch (state)
	{
	case PlayerState::IDLE:    return "IDLE";
	case PlayerState::WALKING: return "WALKING";
	case PlayerState::RUNNING: return "RUNNING";
	default:                   return "UNKNOWN";
	}
}

const char* WeaponStateToString(WeaponState state)
{
	switch (state)
	{
	case WeaponState::HIP:                  return "HIP";
	case WeaponState::HIP_FIRING:           return "HIP_FIRING";
	case WeaponState::ADS_FIRING:           return "ADS_FIRING";
	case WeaponState::ADS_IN:               return "ADS_IN";
	case WeaponState::ADS_OUT:              return "ADS_OUT";
	case WeaponState::ADS:                  return "ADS";
	case WeaponState::RELOADING:            return "RELOADING";
	case WeaponState::RELOADING_OUT_OF_AMMO:return "RELOADING_OUT_OF_AMMO";
	case WeaponState::INSPECTING:           return "INSPECTING";
	case WeaponState::TAKING_OUT:           return "TAKING_OUT";
	default:                                return "UNKNOWN";
	}
}

//=============================================================================
// BuildTables — Animation config & auto-transition data
//=============================================================================
void PlayerStateMachine::BuildTables()
{
	// ---- Player movement → animation ----
	//                                       index  loop   blend
	m_PlayerAnimTable[PlayerState::IDLE]    = { 0,  true,  0.1f };
	m_PlayerAnimTable[PlayerState::WALKING] = { 1,  true,  0.1f };
	m_PlayerAnimTable[PlayerState::RUNNING] = { 2,  true,  0.1f };

	// ---- Weapon state → animation (overrides movement when not HIP) ----
	//                                                       index  loop   blend
	m_WeaponAnimTable[WeaponState::HIP_FIRING]             = { 3,  false, 0.1f };
	m_WeaponAnimTable[WeaponState::ADS_IN]                 = { 4,  false, 0.1f };
	m_WeaponAnimTable[WeaponState::ADS_OUT]                = { 5,  false, 0.1f };
	m_WeaponAnimTable[WeaponState::ADS]                    = { 6,  true,  0.1f };
	m_WeaponAnimTable[WeaponState::ADS_FIRING]             = { 7,  true,  0.1f };
	m_WeaponAnimTable[WeaponState::RELOADING]              = { 9,  false, 0.1f };
	m_WeaponAnimTable[WeaponState::RELOADING_OUT_OF_AMMO]  = { 10, false, 0.1f };
	m_WeaponAnimTable[WeaponState::INSPECTING]             = { 11, true,  0.1f };
	m_WeaponAnimTable[WeaponState::TAKING_OUT]             = { 12, false, 0.1f };
	// Note: HIP has no entry — it uses the player movement animation

	// ---- Auto-transitions (one-shot animations that exit automatically) ----
	//                                                progress  nextState            requireNotBlending
	m_AutoTransitions[WeaponState::ADS_IN]       = { 0.3f,     WeaponState::ADS,     false };
	m_AutoTransitions[WeaponState::ADS_OUT]      = { 0.3f,     WeaponState::HIP,     false };
	m_AutoTransitions[WeaponState::RELOADING]    = { 0.9f,     WeaponState::HIP,     false };
	m_AutoTransitions[WeaponState::TAKING_OUT]   = { 0.8f,     WeaponState::HIP,     false };
	m_AutoTransitions[WeaponState::HIP_FIRING]   = { 0.70f,    WeaponState::HIP,     true  };
}

//=============================================================================
// Constructor
//=============================================================================
PlayerStateMachine::PlayerStateMachine()
	: m_PlayerState(PlayerState::IDLE)
	, m_WeaponState(WeaponState::TAKING_OUT)
{
	BuildTables();
}

//=============================================================================
// Getters / Setters
//=============================================================================
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

//=============================================================================
// Update — Data-driven animation selection
//=============================================================================
void PlayerStateMachine::Update(double elapsed_time, Animator* animator)
{
	// ----------------------------------------------------------------
	// 1. Auto-transitions: check if a one-shot animation is done
	// ----------------------------------------------------------------
	auto transIt = m_AutoTransitions.find(m_WeaponState);
	if (transIt != m_AutoTransitions.end())
	{
		const AutoTransition& trans = transIt->second;

		// Find the expected animation index for the current weapon state
		auto weapAnimIt = m_WeaponAnimTable.find(m_WeaponState);
		if (weapAnimIt != m_WeaponAnimTable.end())
		{
			int expectedIndex = weapAnimIt->second.animIndex;

			// Only transition if the animator is actually playing this animation
			if (animator->GetCurrentAnimationIndex() == expectedIndex &&
				animator->GetCurrAniProgress() > trans.progressThreshold)
			{
				// Extra condition: some transitions require not blending
				if (!trans.requireNotBlending || !animator->IsBlending())
				{
					m_WeaponState = trans.nextState;
				}
			}
		}
	}

	// ----------------------------------------------------------------
	// 2. Determine animation index from tables
	// ----------------------------------------------------------------
	// Start with player movement animation
	const AnimConfig& playerAnim = m_PlayerAnimTable[m_PlayerState];
	int   animIndex = playerAnim.animIndex;
	bool  loop      = playerAnim.loop;
	float blend     = playerAnim.blendTime;

	// Weapon state overrides (unless HIP, which has no entry)
	auto weapIt = m_WeaponAnimTable.find(m_WeaponState);
	if (weapIt != m_WeaponAnimTable.end())
	{
		animIndex = weapIt->second.animIndex;
		loop      = weapIt->second.loop;
		blend     = weapIt->second.blendTime;
	}

	// ----------------------------------------------------------------
	// 3. Play & update
	// ----------------------------------------------------------------
	animator->PlayCrossFade(animIndex, loop, blend);
	animator->Update(elapsed_time);
}
