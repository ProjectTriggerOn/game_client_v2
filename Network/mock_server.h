#pragma once
//=============================================================================
// mock_server.h
//
// Mock Server with fixed-tick (32Hz) game logic.
// Uses accumulator pattern to decouple from render frame rate.
//
// Architecture:
//   - Server runs at fixed 32Hz tick rate
//   - Consumes InputCmd from client
//   - Produces authoritative PlayerState
//   - Broadcasts Snapshot to client
//=============================================================================

#include "net_common.h"
#include "collision_world.h"

class INetwork;

class MockServer
{
public:
    //-------------------------------------------------------------------------
    // Constants
    //-------------------------------------------------------------------------
    static constexpr double TICK_RATE = 32.0;                    // 32 ticks per second
    static constexpr double TICK_DURATION = 1.0 / TICK_RATE;     // ~31.25ms per tick

    MockServer();
    ~MockServer();

    void Initialize(INetwork* pNetwork, CollisionWorld* pCollisionWorld = nullptr);
    void Finalize();

    //-------------------------------------------------------------------------
    // Called every render frame - uses accumulator for fixed tick
    //-------------------------------------------------------------------------
    void Update(double deltaTime);

    //-------------------------------------------------------------------------
    // Getters for debug visualization
    //-------------------------------------------------------------------------
    uint32_t GetCurrentTick() const { return m_CurrentTick; }
    double GetAccumulator() const { return m_Accumulator; }
    double GetServerTime() const { return m_ServerTime; }
    const NetPlayerState& GetPlayerState() const { return m_PlayerState; }

private:
    //-------------------------------------------------------------------------
    // Fixed tick logic (called at exactly 32Hz)
    //-------------------------------------------------------------------------
    void Tick();

    //-------------------------------------------------------------------------
    // Process single input command
    //-------------------------------------------------------------------------
    void ProcessInputCmd(const InputCmd& cmd);

    //-------------------------------------------------------------------------
    // Apply physics simulation for one tick
    //-------------------------------------------------------------------------
    void SimulatePhysics();

    //-------------------------------------------------------------------------
    // Send snapshot to client
    //-------------------------------------------------------------------------
    void BroadcastSnapshot();

    //-------------------------------------------------------------------------
    // Combat: fire-rate gating + hitscan raycast
    //-------------------------------------------------------------------------
    void ProcessFiring();

private:
    INetwork* m_pNetwork;

    // Timing
    double m_Accumulator;           // Time accumulated since last tick
    double m_ServerTime;            // Total server time
    uint32_t m_CurrentTick;         // Current tick number

    // Game State (Server Authoritative)
    NetPlayerState m_PlayerState;
    InputCmd m_LastInputCmd;        // Most recent input from client

    // Reload latch timer — keeps IS_RELOADING active for full animation duration
    static constexpr double RELOAD_DURATION = 10.0;  // seconds
    double m_ReloadTimer = 0.0;

    // Collision world for gravity
    CollisionWorld* m_pCollisionWorld = nullptr;

    // Remote bot player (standalone, not mirrored)
    NetPlayerState m_RemotePlayerState{};
    uint8_t  m_RemoteHealth = 200;
    double   m_RemoteRespawnTimer = 0.0;

    // Local player combat state
    double   m_FireTimer = 0.0;
    uint16_t m_FireCounter = 0;

    // Player collision parameters (must match Player_Fps)
    static constexpr float PLAYER_HEIGHT = 1.6f;
    static constexpr float CAPSULE_RADIUS = 0.3f;

    // Weapon parameters (local player = RED team)
    static constexpr double RED_RPM = 600.0;
    static constexpr uint8_t RED_DAMAGE = 34;
    static constexpr uint8_t MAX_HEALTH = 200;
    static constexpr double RESPAWN_TIME = 2.0;
};
