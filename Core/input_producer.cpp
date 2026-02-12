//=============================================================================
// input_producer.cpp
//
// Samples input from KeyLogger/MSLogger and generates InputCmd.
// This is the CLIENT-SIDE input handler that captures player intent
// and sends it to the server.
//=============================================================================

#include "input_producer.h"
#include "mock_network.h"
#include "mock_server.h"
#include "key_logger.h"
#include "ms_logger.h"
#include "player_cam_fps.h"

// Global accessor
InputProducer* g_pInputProducer = nullptr;

InputProducer::InputProducer()
    : m_pNetwork(nullptr)
    , m_TargetTick(0)
    , m_MoveAxisX(0.0f)
    , m_MoveAxisY(0.0f)
    , m_Yaw(0.0f)
    , m_Pitch(0.0f)
    , m_Buttons(InputButtons::NONE)
    , m_JumpPending(false)
    , m_LastCmd{}
{
}

void InputProducer::Initialize(MockNetwork* pNetwork)
{
    m_pNetwork = pNetwork;
    m_TargetTick = 0;
    m_MoveAxisX = 0.0f;
    m_MoveAxisY = 0.0f;
    m_Yaw = 0.0f;
    m_Pitch = 0.0f;
    m_Buttons = InputButtons::NONE;
    m_JumpPending = false;
    m_LastCmd = {};
}

void InputProducer::Finalize()
{
    m_pNetwork = nullptr;
}

//-----------------------------------------------------------------------------
// Update - Called every render frame
// 
// 1. Sample current input state
// 2. Build InputCmd
// 3. Send to server via MockNetwork
//-----------------------------------------------------------------------------
void InputProducer::Update()
{
    if (!m_pNetwork) return;

    // 1. Sample current input
    SampleInput();

    // 2. Build command
    m_LastCmd = BuildInputCmd();

    // 3. Send to server
    m_pNetwork->SendInputCmd(m_LastCmd);

    // 4. Clear sticky jump only when server confirms we're airborne
    //    This ensures jump isn't lost due to frame/tick timing
    extern MockServer* g_pMockServer;
    if (m_JumpPending && g_pMockServer)
    {
        const NetPlayerState& state = g_pMockServer->GetPlayerState();
        // Clear pending if server shows we're jumping (not grounded)
        if ((state.stateFlags & NetStateFlags::IS_JUMPING) ||
            !(state.stateFlags & NetStateFlags::IS_GROUNDED))
        {
            m_JumpPending = false;
        }
    }

    // Increment target tick for next frame
    // (Server will process commands in order)
    m_TargetTick++;
}

//-----------------------------------------------------------------------------
// SampleInput - Read current keyboard and mouse state
//
// Converts discrete key presses into axis values and button flags.
// IMPORTANT: This is the ONLY place that reads input for gameplay.
//-----------------------------------------------------------------------------
void InputProducer::SampleInput()
{
    // ========================================================================
    // Movement Axis (WASD)
    // ========================================================================
    m_MoveAxisX = 0.0f;
    m_MoveAxisY = 0.0f;

    if (KeyLogger_IsPressed(KK_W)) m_MoveAxisY += 1.0f;
    if (KeyLogger_IsPressed(KK_S)) m_MoveAxisY -= 1.0f;
    if (KeyLogger_IsPressed(KK_D)) m_MoveAxisX += 1.0f;
    if (KeyLogger_IsPressed(KK_A)) m_MoveAxisX -= 1.0f;

    // ========================================================================
    // Camera Angles (from PlayerCamFps)
    // These are already computed by the camera system
    // ========================================================================
    DirectX::XMFLOAT3 camFront = PlayerCamFps_GetFront();
    
    // Convert front vector back to yaw/pitch
    // yaw = atan2(x, z), pitch = asin(y)
    m_Yaw = atan2f(camFront.x, camFront.z);
    m_Pitch = asinf(camFront.y);

    // ========================================================================
    // Buttons
    // ========================================================================
    m_Buttons = InputButtons::NONE;

    // JUMP: Sticky - capture trigger and keep pending until consumed
    if (KeyLogger_IsTrigger(KK_SPACE))
    {
        m_JumpPending = true;
    }
    if (m_JumpPending)
    {
        m_Buttons |= InputButtons::JUMP;
    }
    
    if (isButtonDown(MBT_LEFT))
        m_Buttons |= InputButtons::FIRE;
    
    if (isButtonDown(MBT_RIGHT))
        m_Buttons |= InputButtons::ADS;
    
    if (KeyLogger_IsTrigger(KK_R))
        m_Buttons |= InputButtons::RELOAD;
    
    if (KeyLogger_IsTrigger(KK_E))
        m_Buttons |= InputButtons::INSPECT;
    
    if (KeyLogger_IsPressed(KK_LEFTSHIFT))
        m_Buttons |= InputButtons::SPRINT;
}

//-----------------------------------------------------------------------------
// BuildInputCmd - Create InputCmd from current state
//-----------------------------------------------------------------------------
InputCmd InputProducer::BuildInputCmd() const
{
    InputCmd cmd;
    cmd.tickId = m_TargetTick;
    cmd.moveAxisX = m_MoveAxisX;
    cmd.moveAxisY = m_MoveAxisY;
    cmd.yaw = m_Yaw;
    cmd.pitch = m_Pitch;
    cmd.buttons = m_Buttons;
    return cmd;
}
