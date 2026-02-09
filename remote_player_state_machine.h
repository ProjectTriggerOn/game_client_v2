#pragma once
//=============================================================================
// remote_player_state_machine.h
//
// State machine for remote players (server-controlled entities).
// Similar to PlayerStateMachine but designed for server-authoritative state
// without local input processing.
//=============================================================================

#include <string>
#include "animator.h"

//-----------------------------------------------------------------------------
// RemotePlayerState - Movement state derived from server data
//-----------------------------------------------------------------------------
enum class RemotePlayerState
{
    IDLE,
    WALKING,
    RUNNING,
    JUMPING,
    FALLING,
};

//-----------------------------------------------------------------------------
// RemoteWeaponState - Weapon state for remote player animations
//-----------------------------------------------------------------------------
enum class RemoteWeaponState
{
    HIP,
    HIP_FIRING,
    ADS,
    ADS_FIRING,
    RELOADING,
};

//-----------------------------------------------------------------------------
// RemotePlayerStateMachine
//
// Manages animation state based on server data rather than local input.
// The server sends state flags which are translated into animation states.
//-----------------------------------------------------------------------------
class RemotePlayerStateMachine
{
public:
    RemotePlayerStateMachine();
    ~RemotePlayerStateMachine() = default;

    //-------------------------------------------------------------------------
    // State Setters (called when server state received)
    //-------------------------------------------------------------------------
    void SetPlayerState(RemotePlayerState state);
    void SetWeaponState(RemoteWeaponState state);
    
    //-------------------------------------------------------------------------
    // State Getters
    //-------------------------------------------------------------------------
    RemotePlayerState GetPlayerState() const { return m_PlayerState; }
    RemoteWeaponState GetWeaponState() const { return m_WeaponState; }
    
    //-------------------------------------------------------------------------
    // Derive state from velocity (helper for server snapshots)
    //-------------------------------------------------------------------------
    void DeriveStateFromVelocity(float velX, float velZ, float velY, bool isGrounded);
    
    //-------------------------------------------------------------------------
    // Update animation based on current state
    //-------------------------------------------------------------------------
    void Update(double elapsed_time, Animator* animator);
    
    //-------------------------------------------------------------------------
    // Get current animation name for debug
    //-------------------------------------------------------------------------
    std::string GetPlayerStateString() const;
    std::string GetWeaponStateString() const;

private:
    RemotePlayerState m_PlayerState;
    RemoteWeaponState m_WeaponState;
    float m_AccumulatedTime;
};
