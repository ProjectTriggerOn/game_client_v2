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

    // Initialize player state
    m_PlayerState.tickId = 0;
    m_PlayerState.position = { 0.0f, 0.0f, 0.0f };
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
// SimulatePhysics - Server-side physics simulation
// 
// This is a placeholder for Phase 3.
// Currently just demonstrates the tick running.
// Will be filled with actual physics from Player_Fps::Update.
//-----------------------------------------------------------------------------
void MockServer::SimulatePhysics()
{
    // TODO Phase 3: Move physics simulation from Player_Fps here
    // For now, just update position based on simple input
    // This is placeholder logic to demonstrate the tick is running

    const float moveSpeed = 5.0f * static_cast<float>(TICK_DURATION);
    
    // Calculate movement direction from input
    float yaw = m_LastInputCmd.yaw;
    float frontX = sinf(yaw);
    float frontZ = cosf(yaw);
    float rightX = frontZ;
    float rightZ = -frontX;

    // Apply movement from input axis
    float dx = m_LastInputCmd.moveAxisX * rightX + m_LastInputCmd.moveAxisY * frontX;
    float dz = m_LastInputCmd.moveAxisX * rightZ + m_LastInputCmd.moveAxisY * frontZ;

    m_PlayerState.position.x += dx * moveSpeed;
    m_PlayerState.position.z += dz * moveSpeed;

    // Simple gravity placeholder
    // TODO: Proper physics in Phase 3
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
