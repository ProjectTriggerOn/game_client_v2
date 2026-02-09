//=============================================================================
// remote_player_state_machine.cpp
//
// Implementation of RemotePlayerStateMachine.
// Derives animation state from server-provided velocity and state flags.
//=============================================================================

#include "remote_player_state_machine.h"
#include <cmath>

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
RemotePlayerStateMachine::RemotePlayerStateMachine()
    : m_PlayerState(RemotePlayerState::IDLE)
    , m_WeaponState(RemoteWeaponState::HIP)
    , m_AccumulatedTime(0.0f)
{
}

//-----------------------------------------------------------------------------
// SetPlayerState
//-----------------------------------------------------------------------------
void RemotePlayerStateMachine::SetPlayerState(RemotePlayerState state)
{
    m_PlayerState = state;
}

//-----------------------------------------------------------------------------
// SetWeaponState
//-----------------------------------------------------------------------------
void RemotePlayerStateMachine::SetWeaponState(RemoteWeaponState state)
{
    m_WeaponState = state;
}

//-----------------------------------------------------------------------------
// DeriveStateFromVelocity
//
// Automatically determine player movement state from velocity values.
// This is called when receiving server snapshots.
//-----------------------------------------------------------------------------
void RemotePlayerStateMachine::DeriveStateFromVelocity(float velX, float velZ, float velY, bool isGrounded)
{
    // Calculate horizontal speed
    float horizontalSpeed = sqrtf(velX * velX + velZ * velZ);
    
    // Thresholds
    constexpr float IDLE_THRESHOLD = 0.5f;
    constexpr float WALK_THRESHOLD = 3.0f;
    constexpr float RUN_THRESHOLD = 6.0f;
    
    if (!isGrounded)
    {
        // Airborne states
        if (velY > 0.5f)
        {
            m_PlayerState = RemotePlayerState::JUMPING;
        }
        else
        {
            m_PlayerState = RemotePlayerState::FALLING;
        }
    }
    else
    {
        // Grounded states based on horizontal speed
        if (horizontalSpeed < IDLE_THRESHOLD)
        {
            m_PlayerState = RemotePlayerState::IDLE;
        }
        else if (horizontalSpeed < RUN_THRESHOLD)
        {
            m_PlayerState = RemotePlayerState::WALKING;
        }
        else
        {
            m_PlayerState = RemotePlayerState::RUNNING;
        }
    }
}

//-----------------------------------------------------------------------------
// Update - Drive animator based on current state
//-----------------------------------------------------------------------------
void RemotePlayerStateMachine::Update(double elapsed_time, Animator* animator)
{
    if (!animator) return;
    
    m_AccumulatedTime += static_cast<float>(elapsed_time);
    
    int animationIndex = 0;
    bool isLoopAnimation = true;
    float blendTime = 0.15f;
    
    // Map player state to animation index
    switch (m_PlayerState)
    {
    case RemotePlayerState::IDLE:
        animationIndex = 0;
        isLoopAnimation = true;
        break;
    case RemotePlayerState::WALKING:
        animationIndex = 1;
        isLoopAnimation = true;
        break;
    case RemotePlayerState::RUNNING:
        animationIndex = 2;
        isLoopAnimation = true;
        break;
    case RemotePlayerState::JUMPING:
        // Use idle or a jump animation if available
        animationIndex = 0;
        isLoopAnimation = false;
        break;
    case RemotePlayerState::FALLING:
        // Use idle or a fall animation if available
        animationIndex = 0;
        isLoopAnimation = false;
        break;
    }
    
    // Weapon state can override animation
    switch (m_WeaponState)
    {
    case RemoteWeaponState::HIP:
        // Keep movement animation
        break;
    case RemoteWeaponState::HIP_FIRING:
        animationIndex = 3;
        isLoopAnimation = false;
        break;
    case RemoteWeaponState::ADS:
        animationIndex = 6;
        isLoopAnimation = true;
        break;
    case RemoteWeaponState::ADS_FIRING:
        animationIndex = 7;
        isLoopAnimation = true;
        break;
    case RemoteWeaponState::RELOADING:
        animationIndex = 9;
        isLoopAnimation = false;
        break;
    }
    
    // Play animation with crossfade
    animator->PlayCrossFade(animationIndex, isLoopAnimation, blendTime);
    animator->Update(elapsed_time);
}

//-----------------------------------------------------------------------------
// GetPlayerStateString - Debug helper
//-----------------------------------------------------------------------------
std::string RemotePlayerStateMachine::GetPlayerStateString() const
{
    switch (m_PlayerState)
    {
    case RemotePlayerState::IDLE: return "IDLE";
    case RemotePlayerState::WALKING: return "WALKING";
    case RemotePlayerState::RUNNING: return "RUNNING";
    case RemotePlayerState::JUMPING: return "JUMPING";
    case RemotePlayerState::FALLING: return "FALLING";
    default: return "UNKNOWN";
    }
}

//-----------------------------------------------------------------------------
// GetWeaponStateString - Debug helper
//-----------------------------------------------------------------------------
std::string RemotePlayerStateMachine::GetWeaponStateString() const
{
    switch (m_WeaponState)
    {
    case RemoteWeaponState::HIP: return "HIP";
    case RemoteWeaponState::HIP_FIRING: return "FIRING";
    case RemoteWeaponState::ADS: return "ADS";
    case RemoteWeaponState::ADS_FIRING: return "ADS_FIRE";
    case RemoteWeaponState::RELOADING: return "RELOAD";
    default: return "UNKNOWN";
    }
}
