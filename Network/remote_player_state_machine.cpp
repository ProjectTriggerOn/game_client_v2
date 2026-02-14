//=============================================================================
// remote_player_state_machine.cpp
//
// Implementation of RemotePlayerStateMachine.
// Derives animation state from server-provided velocity and state flags.
//=============================================================================

#include "remote_player_state_machine.h"
#include <cmath>
#include <DirectXMath.h> // For XM_* constants and math

using namespace DirectX;

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
RemotePlayerStateMachine::RemotePlayerStateMachine()
    : m_PlayerState(RemotePlayerState::IDLE)
    , m_WeaponState(RemoteWeaponState::NONE)
    , m_MoveDirection(RemoteMoveDirection::NONE)
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
void RemotePlayerStateMachine::DeriveStateFromVelocity(float velX, float velZ, float velY, bool isGrounded, float yaw)
{
    // Calculate horizontal speed
    float horizontalSpeed = sqrtf(velX * velX + velZ * velZ);
    
    // Thresholds
    constexpr float IDLE_THRESHOLD = 0.5f;
    constexpr float WALK_THRESHOLD = 3.0f;
    constexpr float RUN_THRESHOLD = 6.0f;
    
    // Determine movement direction relative to facing
    if (horizontalSpeed > IDLE_THRESHOLD)
    {
        m_MoveDirection = CalculateDirection(velX, velZ, yaw);
    }
    else
    {
        m_MoveDirection = RemoteMoveDirection::NONE;
    }

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
// CalculateDirection
// Calculates 8-way direction from velocity relative to character yaw
//-----------------------------------------------------------------------------
RemoteMoveDirection RemotePlayerStateMachine::CalculateDirection(float velX, float velZ, float yaw)
{
    // 1. Calculate the angle of the velocity vector (World Space)
    // atan2 returns angle in radians [-PI, PI], 0 is +X (East), PI/2 is +Z (North)
    // Standard Math: 0=Right, 90=Up
    // Game World usually: +Z is Forward, +X is Right
    float moveAngle = atan2f(velX, velZ); // using (x, z) means 0 is +Z (North), PI/2 is +X (East) if we follow standard setup? 
    // Actually atan2(y, x) is standard. So atan2(velX, velZ) -> Z is "X axis" (0 deg), X is "Y axis" (90 deg). 
    // Meaning 0 is Forward (+Z), PI/2 is Right (+X). This matches standard 3D forward.

    // 2. Adjust by player's facing yaw
    // We want the angle relative to the player.
    // If player faces North (0) and moves North (0), relative is 0.
    // If player faces East (PI/2) and moves North (0), relative is -PI/2 (Left).
    float probYaw = yaw; 
    
    // Typically yaw in game engines: 0 might be +Z. 
    // Let's assume standard logic: relative = move - yaw.
    float relativeAngle = moveAngle - probYaw;

    // Normalize to [-PI, PI]
    while (relativeAngle > XM_PI) relativeAngle -= XM_2PI;
    while (relativeAngle < -XM_PI) relativeAngle += XM_2PI;

    // 3. Map to 8 directions (45 degrees / PI/4 per sector)
    // Slices are centered on directions.
    // Forward (0): [-PI/8, PI/8]
    // Forward-Right (PI/4): [PI/8, 3PI/8]
    // Right (PI/2): [3PI/8, 5PI/8]
    // ...
    
    float deg = XMConvertToDegrees(relativeAngle);
    
    // Shift so 0 is 0..45, 1 is 45..90? 
    // Easier to offset by 22.5 (half slice) and floor.
    // Range [-180, 180]. Add 180+22.5 = 202.5. Range [22.5, 382.5]. / 45.
    
    float shifted = deg + 180.0f + 22.5f;
    if (shifted >= 360.0f) shifted -= 360.0f;
    
    int sector = static_cast<int>(shifted / 45.0f);
    // 0: Back (-180)
    // 1: Back-Left (-135) 
    // 2: Left (-90)
    // 3: Forward-Left (-45)
    // 4: Forward (0)
    // 5: Forward-Right (45)
    // 6: Right (90)
    // 7: Back-Right (135)
    // Note: atan2(x, z) -> Z=0, X=90.
    // relative = move - yaw.
    // If relative is 0 => Forward.
    
    // Let's re-verify the sector mapping logic relative to 0.
    // 0 deg +/- 22.5 -> Forward.
    // 45 deg +/- 22.5 -> Forward-Right
    // 90 deg +/- 22.5 -> Right (if +X is Right)
    // etc.
    // My previous calc assumed -180 start.
    // simpler: add 360 to handle negatives easily, then offset.
    
    float angleDeg = XMConvertToDegrees(relativeAngle); // -180 to 180
    if (angleDeg < 0) angleDeg += 360.0f; // 0 to 360. 0 is Forward.
    
    // 0 is Forward.
    // 337.5 ~ 22.5 : Forward
    // 22.5 ~ 67.5 : Forward-Right
    // 67.5 ~ 112.5 : Right
    // 112.5 ~ 157.5 : Back-Right
    // 157.5 ~ 202.5 : Backward
    // 202.5 ~ 247.5 : Back-Left
    // 247.5 ~ 292.5 : Left
    // 292.5 ~ 337.5 : Forward-Left
    
    angleDeg += 22.5f;
    if (angleDeg >= 360.0f) angleDeg -= 360.0f;
    
    int octant = static_cast<int>(angleDeg / 45.0f);
    
    switch (octant)
    {
    case 0: return RemoteMoveDirection::FORWARD;
    case 1: return RemoteMoveDirection::FORWARD_RIGHT; // Assuming +Angle is Right
    case 2: return RemoteMoveDirection::RIGHT;
    case 3: return RemoteMoveDirection::BACKWARD_RIGHT;
    case 4: return RemoteMoveDirection::BACKWARD;
    case 5: return RemoteMoveDirection::BACKWARD_LEFT;
    case 6: return RemoteMoveDirection::LEFT;
    case 7: return RemoteMoveDirection::FORWARD_LEFT;
    default: return RemoteMoveDirection::FORWARD;
    }
}


//-----------------------------------------------------------------------------
// Update - Drive animator based on current state
//-----------------------------------------------------------------------------
void RemotePlayerStateMachine::Update(double elapsed_time, Animator* animator)
{
    if (!animator) return;
    
    m_AccumulatedTime += static_cast<float>(elapsed_time);
    
    // ================================================================
    // BASE LAYER (lower body): always driven by movement state
    // ================================================================
    int animationIndex = ANI_IDLE;
    bool isLoopAnimation = true;
    float blendTime = 0.2f;
    
    switch (m_PlayerState)
    {
    case RemotePlayerState::IDLE:
        animationIndex = ANI_IDLE;
        break;
        
    case RemotePlayerState::WALKING:
        switch (m_MoveDirection)
        {
        case RemoteMoveDirection::FORWARD:        animationIndex = ANI_WALK_FORWARD; break;
        case RemoteMoveDirection::BACKWARD:       animationIndex = ANI_WALK_BACKWARD; break;
        case RemoteMoveDirection::LEFT:           animationIndex = ANI_WALK_STRAFE_LEFT; break;
        case RemoteMoveDirection::RIGHT:          animationIndex = ANI_WALK_STRAFE_RIGHT; break;
        case RemoteMoveDirection::FORWARD_LEFT:   animationIndex = ANI_WALK_45_UP_LEFT; break;
        case RemoteMoveDirection::FORWARD_RIGHT:  animationIndex = ANI_WALK_45_UP_RIGHT; break;
        case RemoteMoveDirection::BACKWARD_LEFT:  animationIndex = ANI_WALK_45_BACK_LEFT; break;
        case RemoteMoveDirection::BACKWARD_RIGHT: animationIndex = ANI_WALK_45_BACK_RIGHT; break;
        default:                                  animationIndex = ANI_WALK_FORWARD; break;
        }
        break;
        
    case RemotePlayerState::RUNNING:
        switch (m_MoveDirection)
        {
        case RemoteMoveDirection::FORWARD:        animationIndex = ANI_RUN_FORWARD; break;
        case RemoteMoveDirection::BACKWARD:       animationIndex = ANI_RUN_BACKWARD; break;
        case RemoteMoveDirection::LEFT:           animationIndex = ANI_RUN_STRAFE_LEFT; break;
        case RemoteMoveDirection::RIGHT:          animationIndex = ANI_RUN_STRAFE_RIGHT; break;
        case RemoteMoveDirection::FORWARD_LEFT:   animationIndex = ANI_RUN_45_UP_LEFT; break;
        case RemoteMoveDirection::FORWARD_RIGHT:  animationIndex = ANI_RUN_45_UP_RIGHT; break;
        case RemoteMoveDirection::BACKWARD_LEFT:  animationIndex = ANI_RUN_45_BACK_LEFT; break;
        case RemoteMoveDirection::BACKWARD_RIGHT: animationIndex = ANI_RUN_45_BACK_RIGHT; break;
        default:                                  animationIndex = ANI_RUN_FORWARD; break;
        }
        break;
        
    case RemotePlayerState::JUMPING:
        animationIndex = ANI_JUMP_LOOP;
        isLoopAnimation = true;
        break;
        
    case RemotePlayerState::FALLING:
        animationIndex = ANI_JUMP_LAND;
        isLoopAnimation = false;
        break;
    }
    
    // Play base layer (movement) with crossfade
    animator->PlayCrossFade(animationIndex, isLoopAnimation, blendTime);
    
    // ================================================================
    // ADDITIVE LAYER (upper body): weapon actions overlaid on movement
    // ================================================================
    switch (m_WeaponState)
    {
    case RemoteWeaponState::FIRING:
    {
        // RPM-based fire: first shot fires immediately, subsequent at interval
        if (!m_WasFiring)
        {
            // Just started firing — play immediately
            animator->PlayAdditive(ANI_FIRE, false, 1.0f);
            m_FireTimer = 0.0f;
            m_WasFiring = true;
        }
        else
        {
            // Continuous fire — accumulate timer
            m_FireTimer += static_cast<float>(elapsed_time);
            if (m_FireTimer >= FIRE_INTERVAL)
            {
                m_FireTimer -= FIRE_INTERVAL;
                animator->PlayAdditive(ANI_FIRE, false, 1.0f);
            }
        }
        break;
    }
    case RemoteWeaponState::RELOAD:
        if (m_WasFiring) m_WasFiring = false;
        if (!m_WasReloading)
        {
            animator->PlayAdditive(ANI_RELOAD_AMMO_LEFT, false, 1.0f);
            m_WasReloading = true;
        }
        break;
    case RemoteWeaponState::RELOAD_EMPTY:
        if (m_WasFiring) m_WasFiring = false;
        if (!m_WasReloading)
        {
            animator->PlayAdditive(ANI_RELOAD_OUT_OF_AMMO, false, 1.0f);
            m_WasReloading = true;
        }
        break;
    case RemoteWeaponState::NONE:
    default:
        if (m_WasFiring) m_WasFiring = false;
        if (m_WasReloading) m_WasReloading = false;
        if (animator->IsAdditiveActive())
            animator->StopAdditive(0.15);
        break;
    }
    
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
    case RemoteWeaponState::NONE:         return "NONE";
    case RemoteWeaponState::FIRING:       return "FIRING";
    case RemoteWeaponState::RELOAD:       return "RELOAD";
    case RemoteWeaponState::RELOAD_EMPTY: return "RELOAD_EMPTY";
    default: return "UNKNOWN";
    }
}

std::string RemotePlayerStateMachine::GetMoveDirectionString() const
{
    switch (m_MoveDirection)
    {
    case RemoteMoveDirection::NONE: return "NONE";
    case RemoteMoveDirection::FORWARD: return "FWD";
    case RemoteMoveDirection::BACKWARD: return "BWD";
    case RemoteMoveDirection::LEFT: return "LEFT";
    case RemoteMoveDirection::RIGHT: return "RIGHT";
    case RemoteMoveDirection::FORWARD_LEFT: return "FWD_L";
    case RemoteMoveDirection::FORWARD_RIGHT: return "FWD_R";
    case RemoteMoveDirection::BACKWARD_LEFT: return "BWD_L";
    case RemoteMoveDirection::BACKWARD_RIGHT: return "BWD_R";
    default: return "UNK";
    }
}
