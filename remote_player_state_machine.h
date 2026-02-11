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
// RemoteMoveDirection - 8-way directional movement
//-----------------------------------------------------------------------------
enum class RemoteMoveDirection
{
    NONE,
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    FORWARD_LEFT,
    FORWARD_RIGHT,
    BACKWARD_LEFT,
    BACKWARD_RIGHT,
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

    //-----------------------------------------------------------------------------
    // Constants for Animation Indices
    //-----------------------------------------------------------------------------
    static constexpr int ANI_POSE_REF = 0;
    static constexpr int ANI_REFERENCE = 1;
    static constexpr int ANI_FIRE = 2;
    static constexpr int ANI_IDLE = 3;
    static constexpr int ANI_IDLE_RELAXED = 4;
    static constexpr int ANI_JUMP_LAND = 5;
    static constexpr int ANI_JUMP_LOOP = 6;
    static constexpr int ANI_JUMP_START = 7;
    static constexpr int ANI_RELOAD_AMMO_LEFT = 8;
    static constexpr int ANI_RELOAD_OUT_OF_AMMO = 9;
    
    // Run animations (8 dynamic directions)
    static constexpr int ANI_RUN_45_BACK_LEFT = 10;
    static constexpr int ANI_RUN_45_BACK_RIGHT = 11;
    static constexpr int ANI_RUN_45_UP_LEFT = 12;
    static constexpr int ANI_RUN_45_UP_RIGHT = 13;
    static constexpr int ANI_RUN_BACKWARD = 14;
    static constexpr int ANI_RUN_FORWARD = 15;
    static constexpr int ANI_RUN_STRAFE_LEFT = 16;
    static constexpr int ANI_RUN_STRAFE_RIGHT = 17;

    // Walk animations (8 dynamic directions)
    static constexpr int ANI_WALK_45_BACK_LEFT = 18;
    static constexpr int ANI_WALK_45_BACK_RIGHT = 19;
    static constexpr int ANI_WALK_45_UP_LEFT = 20;
    static constexpr int ANI_WALK_45_UP_RIGHT = 21;
    static constexpr int ANI_WALK_BACKWARD = 22;
    static constexpr int ANI_WALK_FORWARD = 23;
    static constexpr int ANI_WALK_STRAFE_LEFT = 24;
    static constexpr int ANI_WALK_STRAFE_RIGHT = 25;

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
    RemoteMoveDirection GetMoveDirection() const { return m_MoveDirection; }
    
    //-------------------------------------------------------------------------
    // Derive state from velocity (helper for server snapshots)
    //-------------------------------------------------------------------------
    void DeriveStateFromVelocity(float velX, float velZ, float velY, bool isGrounded, float yaw);
    
    //-------------------------------------------------------------------------
    // Update animation based on current state
    //-------------------------------------------------------------------------
    void Update(double elapsed_time, Animator* animator);
    
    //-------------------------------------------------------------------------
    // Get current animation name for debug
    //-------------------------------------------------------------------------
    std::string GetPlayerStateString() const;
    std::string GetWeaponStateString() const;
    std::string GetMoveDirectionString() const;

private:
    RemotePlayerState m_PlayerState;
    RemoteWeaponState m_WeaponState;
    RemoteMoveDirection m_MoveDirection;
    float m_AccumulatedTime;
    
    // Helper to calculate direction from velocity relative to yaw
    RemoteMoveDirection CalculateDirection(float velX, float velZ, float yaw);
};
