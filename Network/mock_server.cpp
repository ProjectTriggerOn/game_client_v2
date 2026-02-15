//=============================================================================
// mock_server.cpp
//
// Mock Server implementation with fixed 32Hz tick rate.
// Uses accumulator pattern for frame-rate independent simulation.
//=============================================================================

#include "mock_server.h"
#include "i_network.h"
#include <cmath>

MockServer::MockServer()
    : m_pNetwork(nullptr)
    , m_Accumulator(0.0)
    , m_ServerTime(0.0)
    , m_CurrentTick(0)
    , m_PlayerState{}
    , m_LastInputCmd{}
{
}

MockServer::~MockServer()
{
    Finalize();
}

void MockServer::Initialize(INetwork* pNetwork)
{
    m_pNetwork = pNetwork;
    m_Accumulator = 0.0;
    m_ServerTime = 0.0;
    m_CurrentTick = 0;

    // Initialize player state (match Player_Fps spawn in Game_Initialize)
    m_PlayerState.tickId = 0;
    m_PlayerState.position = { 0.0f, 0.0f, -20.0f };  // Same as game.cpp
    m_PlayerState.velocity = { 0.0f, 0.0f, 0.0f };
    m_PlayerState.yaw = 0.0f;
    m_PlayerState.pitch = 0.0f;
    m_PlayerState.stateFlags = NetStateFlags::IS_GROUNDED;

    // Initialize last input
    m_LastInputCmd = {};
}

void MockServer::Finalize()
{
    m_pNetwork = nullptr;
}

//-----------------------------------------------------------------------------
// Update - Called every render frame
// 
// Accumulator Pattern:
//   1. Add frame delta time to accumulator
//   2. While accumulator >= TICK_DURATION, run one Tick()
//   3. This ensures exactly TICK_RATE ticks per second regardless of FPS
//-----------------------------------------------------------------------------
void MockServer::Update(double deltaTime)
{
    if (!m_pNetwork) return;

    // Clamp deltaTime to prevent spiral of death on frame spikes
    const double maxDelta = TICK_DURATION * 4.0;  // Max 4 ticks per frame
    deltaTime = (deltaTime > maxDelta) ? maxDelta : deltaTime;

    m_Accumulator += deltaTime;

    // ========================================================================
    // ACCUMULATOR LOOP - Core of fixed tick timing
    // This loop runs Tick() at exactly 32Hz regardless of render frame rate
    // ========================================================================
    while (m_Accumulator >= TICK_DURATION)
    {
        Tick();
        m_Accumulator -= TICK_DURATION;
    }
}

//-----------------------------------------------------------------------------
// Tick - Fixed rate game logic (32Hz)
// 
// This is where all authoritative game logic runs:
//   1. Consume input commands from client
//   2. Simulate physics
//   3. Update game state
//   4. Broadcast snapshot to client
//-----------------------------------------------------------------------------
void MockServer::Tick()
{
    m_CurrentTick++;
    m_ServerTime += TICK_DURATION;

    // 1. Consume all pending input commands
    //    In a real server, we'd buffer and process inputs properly
    InputCmd cmd;
    while (m_pNetwork->ReceiveInputCmd(cmd))
    {
        ProcessInputCmd(cmd);
    }

    // 2. Simulate physics for this tick
    SimulatePhysics();

    // 3. Update tick ID in state
    m_PlayerState.tickId = m_CurrentTick;

    // 4. Broadcast snapshot to client
    BroadcastSnapshot();
}

//-----------------------------------------------------------------------------
// ProcessInputCmd - Handle input from client
// 
// NOTE: Client only sends INPUT INTENT, never position/velocity.
// Server applies movement based on input.
//-----------------------------------------------------------------------------
void MockServer::ProcessInputCmd(const InputCmd& cmd)
{
    m_LastInputCmd = cmd;

    // Store camera angles
    m_PlayerState.yaw = cmd.yaw;
    m_PlayerState.pitch = cmd.pitch;

    // Update state flags based on buttons
    uint32_t flags = m_PlayerState.stateFlags;

    if (cmd.buttons & InputButtons::FIRE)
        flags |= NetStateFlags::IS_FIRING;
    else
        flags &= ~NetStateFlags::IS_FIRING;

    if (cmd.buttons & InputButtons::ADS)
        flags |= NetStateFlags::IS_ADS;
    else
        flags &= ~NetStateFlags::IS_ADS;

    if (cmd.buttons & InputButtons::RELOAD)
    {
        // Start reload latch — keep IS_RELOADING active for duration
        if (m_ReloadTimer <= 0.0)
            m_ReloadTimer = RELOAD_DURATION;
    }
    
    // Reload latch timer
    if (m_ReloadTimer > 0.0)
    {
        flags |= NetStateFlags::IS_RELOADING;
        m_ReloadTimer -= TICK_DURATION;
        if (m_ReloadTimer <= 0.0)
        {
            m_ReloadTimer = 0.0;
            flags &= ~NetStateFlags::IS_RELOADING;
        }
    }
    else
    {
        flags &= ~NetStateFlags::IS_RELOADING;
    }

    m_PlayerState.stateFlags = flags;
}

//-----------------------------------------------------------------------------
// SimulatePhysics - Server-side authoritative physics simulation
// 
// CS:GO / Valorant Style Movement:
//   - Ground: Snappy, instant response, no sliding
//   - Air: Momentum preservation, limited air control
//   - Target velocity approach instead of force accumulation
//-----------------------------------------------------------------------------
void MockServer::SimulatePhysics()
{
    const float dt = static_cast<float>(TICK_DURATION);
    
    // ========================================================================
    // MOVEMENT PARAMETERS (CS:GO / Valorant style)
    // ========================================================================
    constexpr float MAX_WALK_SPEED = 5.0f;    // Walking speed
    constexpr float MAX_RUN_SPEED  = 8.0f;    // Sprinting speed
    constexpr float GROUND_ACCEL   = 50.0f;   // High = snappy ground control
    constexpr float AIR_ACCEL      = 2.0f;    // Low = limited air control
    constexpr float GRAVITY        = 20.0f;   // Heavy, quick jumps
    constexpr float JUMP_VELOCITY  = 8.0f;    // Jump impulse
    
    bool isGrounded = (m_PlayerState.stateFlags & NetStateFlags::IS_GROUNDED) != 0;
    
    // ========================================================================
    // STEP 1: Calculate Target Velocity from Input
    // ========================================================================
    float yaw = m_LastInputCmd.yaw;
    
    // Forward/Right vectors from yaw (flattened)
    float frontX = sinf(yaw);
    float frontZ = cosf(yaw);
    float rightX = frontZ;
    float rightZ = -frontX;
    
    // Calculate move direction from input
    float moveX = m_LastInputCmd.moveAxisX * rightX + m_LastInputCmd.moveAxisY * frontX;
    float moveZ = m_LastInputCmd.moveAxisX * rightZ + m_LastInputCmd.moveAxisY * frontZ;
    
    // Normalize diagonal movement
    float moveMag = sqrtf(moveX * moveX + moveZ * moveZ);
    if (moveMag > 1.0f)
    {
        moveX /= moveMag;
        moveZ /= moveMag;
        moveMag = 1.0f;
    }
    
    // Target speed based on sprint
    float maxSpeed = (m_LastInputCmd.buttons & InputButtons::SPRINT) ? MAX_RUN_SPEED : MAX_WALK_SPEED;
    
    // Target velocity = normalized direction * max speed
    float targetVelX = moveX * maxSpeed;
    float targetVelZ = moveZ * maxSpeed;
    
    // ========================================================================
    // STEP 2: Ground vs Air Movement
    // ========================================================================
    if (isGrounded)
    {
        // ---------------------------------------------------------------------
        // GROUND: Snappy, high acceleration, instant stop
        // MoveTowards formula: vel = vel + clamp(target - vel, -accel*dt, accel*dt)
        // ---------------------------------------------------------------------
        float accelStep = GROUND_ACCEL * dt;
        
        // X axis
        float diffX = targetVelX - m_PlayerState.velocity.x;
        if (fabsf(diffX) <= accelStep)
            m_PlayerState.velocity.x = targetVelX;
        else
            m_PlayerState.velocity.x += (diffX > 0 ? accelStep : -accelStep);
        
        // Z axis
        float diffZ = targetVelZ - m_PlayerState.velocity.z;
        if (fabsf(diffZ) <= accelStep)
            m_PlayerState.velocity.z = targetVelZ;
        else
            m_PlayerState.velocity.z += (diffZ > 0 ? accelStep : -accelStep);
        
        // ---------------------------------------------------------------------
        // JUMP - Only on ground
        // ---------------------------------------------------------------------
        if (m_LastInputCmd.buttons & InputButtons::JUMP)
        {
            m_PlayerState.velocity.y = JUMP_VELOCITY;
            m_PlayerState.stateFlags &= ~NetStateFlags::IS_GROUNDED;
            m_PlayerState.stateFlags |= NetStateFlags::IS_JUMPING;
            isGrounded = false;
        }
    }
    else
    {
        // ---------------------------------------------------------------------
        // AIR: Preserve momentum, limited air control (air strafing)
        // No friction applied - only slight influence from input
        // ---------------------------------------------------------------------
        float airStep = AIR_ACCEL * dt;
        
        // Only apply air control if there's input
        if (moveMag > 0.01f)
        {
            // Air strafe: add small acceleration in input direction
            m_PlayerState.velocity.x += moveX * airStep;
            m_PlayerState.velocity.z += moveZ * airStep;
            
            // Cap horizontal speed to prevent infinite acceleration
            float horizSpeed = sqrtf(m_PlayerState.velocity.x * m_PlayerState.velocity.x + 
                                     m_PlayerState.velocity.z * m_PlayerState.velocity.z);
            if (horizSpeed > maxSpeed * 1.2f)  // Allow slight overspeed from bunny hop
            {
                float scale = (maxSpeed * 1.2f) / horizSpeed;
                m_PlayerState.velocity.x *= scale;
                m_PlayerState.velocity.z *= scale;
            }
        }
        // NO friction in air - momentum preserved
    }
    
    // ========================================================================
    // STEP 3: Gravity (always applies, even briefly on ground for stability)
    // ========================================================================
    if (!isGrounded)
    {
        m_PlayerState.velocity.y -= GRAVITY * dt;
    }
    
    // ========================================================================
    // STEP 4: Apply velocity to position
    // ========================================================================
    m_PlayerState.position.x += m_PlayerState.velocity.x * dt;
    m_PlayerState.position.z += m_PlayerState.velocity.z * dt;
    m_PlayerState.position.y += m_PlayerState.velocity.y * dt;
    
    // ========================================================================
    // STEP 5: Floor Collision (y = 0)
    // ========================================================================
    if (m_PlayerState.position.y <= 0.0f)
    {
        m_PlayerState.position.y = 0.0f;
        m_PlayerState.velocity.y = 0.0f;
        m_PlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
        m_PlayerState.stateFlags &= ~NetStateFlags::IS_JUMPING;
    }
}

//-----------------------------------------------------------------------------
// BroadcastSnapshot - Send authoritative state to client
//-----------------------------------------------------------------------------
void MockServer::BroadcastSnapshot()
{
    if (!m_pNetwork) return;

    Snapshot snapshot = {};
    snapshot.tickId = m_CurrentTick;
    snapshot.serverTime = m_ServerTime;
    snapshot.localPlayer = m_PlayerState;
    snapshot.localPlayerId = 0;
    snapshot.localPlayerTeam = PlayerTeam::RED;

    // Mirror local player as remote player for TP model debug (offset 3m forward)
    snapshot.remotePlayers[0].playerId = 1;
    snapshot.remotePlayers[0].teamId = PlayerTeam::RED;
    snapshot.remotePlayers[0].state = m_PlayerState;
    snapshot.remotePlayers[0].state.position.x += sinf(m_PlayerState.yaw) * 3.0f;
    snapshot.remotePlayers[0].state.position.z += cosf(m_PlayerState.yaw) * 3.0f;
    snapshot.remotePlayerCount = 1;

    m_pNetwork->SendSnapshot(snapshot);
}

