#pragma once
//=============================================================================
// input_producer.h
//
// Generates InputCmd from current input state every frame.
// Client-side only - converts raw input into network-ready commands.
//
// Data Flow:
//   KeyLogger/MSLogger → InputProducer → InputCmd → MockNetwork → Server
//=============================================================================

#include "net_common.h"

class INetwork;

class InputProducer
{
public:
    InputProducer();
    ~InputProducer() = default;

    void Initialize(INetwork* pNetwork);
    void Finalize();

    //-------------------------------------------------------------------------
    // Called every render frame - samples input each frame, but sends InputCmd
    // at a fixed rate (decoupled from frame rate) so a high-fps client cannot
    // flood the server. deltaTime is this frame's elapsed seconds.
    //-------------------------------------------------------------------------
    void Update(double deltaTime);

    //-------------------------------------------------------------------------
    // Get current input state (for client-side prediction)
    //-------------------------------------------------------------------------
    const InputCmd& GetLastInputCmd() const { return m_LastCmd; }

    //-------------------------------------------------------------------------
    // Feed last server state (for jump-pending logic without MockServer access)
    //-------------------------------------------------------------------------
    void SetLastServerState(const NetPlayerState& state) { m_LastServerState = state; m_HasServerState = true; }

    //-------------------------------------------------------------------------
    // Target tick for synchronization
    //-------------------------------------------------------------------------
    void SetTargetTick(uint32_t tick) { m_TargetTick = tick; }
    uint32_t GetTargetTick() const { return m_TargetTick; }

private:
    //-------------------------------------------------------------------------
    // Sample current keyboard/mouse state
    //-------------------------------------------------------------------------
    void SampleInput();

    //-------------------------------------------------------------------------
    // Build InputCmd from sampled state
    //-------------------------------------------------------------------------
    InputCmd BuildInputCmd() const;

private:
    INetwork* m_pNetwork;
    
    uint32_t m_TargetTick;      // Next server tick to target
    
    // Cached input state
    float m_MoveAxisX;          // -1 to 1 (A/D)
    float m_MoveAxisY;          // -1 to 1 (S/W)
    float m_Yaw;                // Camera yaw (radians)
    float m_Pitch;              // Camera pitch (radians)
    uint32_t m_Buttons;         // Button bitfield

    // Send throttle: decouple the network send rate from the (possibly very high)
    // frame rate. Input is sampled every frame; m_PendingEdges OR-accumulates the
    // one-shot button edges (RELOAD/INSPECT) seen since the last send so a
    // throttled send never drops one.
    double m_SendAccumulator;   // seconds accumulated toward the next send
    uint32_t m_PendingEdges;    // one-shot edges seen since the last send

    bool m_JumpPending;         // Sticky jump: persists until server processes

    InputCmd m_LastCmd;         // Most recent command sent

    NetPlayerState m_LastServerState;  // Last received server state
    bool m_HasServerState;             // Whether we have received any server state
};

// Global accessor (set in main.cpp)
extern InputProducer* g_pInputProducer;
