//=============================================================================
// mock_server.cpp
//
// Mock Server implementation with fixed 32Hz tick rate.
// Uses accumulator pattern for frame-rate independent simulation.
//=============================================================================

#include "mock_server.h"
#include "mock_network.h"
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

void MockServer::Initialize(MockNetwork* pNetwork)
{
    m_pNetwork = pNetwork;
    m_Accumulator = 0.0;
    m_ServerTime = 0.0;
    m_CurrentTick = 0;

    // Initialize player state (match Player_Fps spawn in Game_Initialize)
    m_PlayerState.tickId = 0;
    m_PlayerState.position = { 0.0f, 3.0f, -20.0f };  // Same as game.cpp
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

    m_PlayerState.stateFlags = flags;
}

//-----------------------------------------------------------------------------
// SimulatePhysics - Server-side authoritative physics simulation
// 
// This is the SERVER's version of Player_Fps::Update physics.
// All movement authority is here - client only predicts.
//-----------------------------------------------------------------------------
void MockServer::SimulatePhysics()
{
    const float dt = static_cast<float>(TICK_DURATION);
    
    // ========================================================================
    // JUMP - Only trigger if grounded and JUMP button pressed
    // ========================================================================
    bool isGrounded = (m_PlayerState.stateFlags & NetStateFlags::IS_GROUNDED) != 0;
    
    if ((m_LastInputCmd.buttons & InputButtons::JUMP) && isGrounded)
    {
        m_PlayerState.velocity.y = 15.0f;  // Jump impulse
        m_PlayerState.stateFlags &= ~NetStateFlags::IS_GROUNDED;
        m_PlayerState.stateFlags |= NetStateFlags::IS_JUMPING;
    }
    
    // ========================================================================
    // GRAVITY
    // ========================================================================
    const float gravity = 9.8f * 1.5f;  // Match Player_Fps gravity
    m_PlayerState.velocity.y -= gravity * dt;
    
    // Apply vertical velocity
    m_PlayerState.position.y += m_PlayerState.velocity.y * dt;
    
    // ========================================================================
    // FLOOR COLLISION (y = 0)
    // ========================================================================
    if (m_PlayerState.position.y <= 0.0f)
    {
        m_PlayerState.position.y = 0.0f;
        m_PlayerState.velocity.y = 0.0f;
        m_PlayerState.stateFlags |= NetStateFlags::IS_GROUNDED;
        m_PlayerState.stateFlags &= ~NetStateFlags::IS_JUMPING;
    }
    
    // ========================================================================
    // HORIZONTAL MOVEMENT (from InputCmd)
    // ========================================================================
    float yaw = m_LastInputCmd.yaw;
    
    // Calculate forward/right vectors from yaw (flattened, no pitch)
    float frontX = sinf(yaw);
    float frontZ = cosf(yaw);
    float rightX = frontZ;   // cos(yaw) = perpendicular
    float rightZ = -frontX;  // -sin(yaw) = perpendicular
    
    // Movement direction from input axis
    float moveX = m_LastInputCmd.moveAxisX * rightX + m_LastInputCmd.moveAxisY * frontX;
    float moveZ = m_LastInputCmd.moveAxisX * rightZ + m_LastInputCmd.moveAxisY * frontZ;
    
    // Normalize diagonal movement
    float moveMag = sqrtf(moveX * moveX + moveZ * moveZ);
    if (moveMag > 0.01f)
    {
        moveX /= moveMag;
        moveZ /= moveMag;
        
        // Speed: walking = 30, running = 40 (per second)
        float speed = (m_LastInputCmd.buttons & InputButtons::SPRINT) ? 40.0f : 30.0f;
        
        m_PlayerState.velocity.x += moveX * speed * dt;
        m_PlayerState.velocity.z += moveZ * speed * dt;
    }
    
    // ========================================================================
    // FRICTION / DAMPING (horizontal only)
    // ========================================================================
    const float friction = 4.0f;
    m_PlayerState.velocity.x -= m_PlayerState.velocity.x * friction * dt;
    m_PlayerState.velocity.z -= m_PlayerState.velocity.z * friction * dt;
    
    // Apply horizontal velocity
    m_PlayerState.position.x += m_PlayerState.velocity.x * dt;
    m_PlayerState.position.z += m_PlayerState.velocity.z * dt;
}

//-----------------------------------------------------------------------------
// BroadcastSnapshot - Send authoritative state to client
//-----------------------------------------------------------------------------
void MockServer::BroadcastSnapshot()
{
    if (!m_pNetwork) return;

    Snapshot snapshot;
    snapshot.tickId = m_CurrentTick;
    snapshot.serverTime = m_ServerTime;
    snapshot.localPlayer = m_PlayerState;

    m_pNetwork->SendSnapshot(snapshot);
}

