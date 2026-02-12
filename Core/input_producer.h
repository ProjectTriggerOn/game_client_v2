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
    // Called every render frame - samples input and sends InputCmd
    //-------------------------------------------------------------------------
    void Update();

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
    
    bool m_JumpPending;         // Sticky jump: persists until server processes

    InputCmd m_LastCmd;         // Most recent command sent

    NetPlayerState m_LastServerState;  // Last received server state
    bool m_HasServerState;             // Whether we have received any server state
};

// Global accessor (set in main.cpp)
extern InputProducer* g_pInputProducer;
