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

class MockNetwork;

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

    void Initialize(MockNetwork* pNetwork);
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

private:
    MockNetwork* m_pNetwork;
    
    // Timing
    double m_Accumulator;           // Time accumulated since last tick
    double m_ServerTime;            // Total server time
    uint32_t m_CurrentTick;         // Current tick number

    // Game State (Server Authoritative)
    NetPlayerState m_PlayerState;
    InputCmd m_LastInputCmd;        // Most recent input from client
};
