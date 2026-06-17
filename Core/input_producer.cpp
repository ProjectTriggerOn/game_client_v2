//=============================================================================
// input_producer.cpp
//
// Samples input from KeyLogger/MSLogger and generates InputCmd.
// This is the CLIENT-SIDE input handler that captures player intent
// and sends it to the server.
//=============================================================================

#include "input_producer.h"
#include "debug_log.h"
#include "game.h"
#include "i_network.h"
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

void InputProducer::Initialize(INetwork* pNetwork)
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
    m_LastServerState = {};
    m_HasServerState = false;
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

    // Log every InputCmd that carries the JUMP bit so we can trace the full pipeline:
    //   SPACE_TRIG  -> SENT (one or more) -> CONSUMED_BY_SERVER -> LOCAL_PREDICT_JUMP
    if (DebugLog_IsEnabled() && DebugLog_LogJumpEvents() &&
        (m_LastCmd.buttons & InputButtons::JUMP))
    {
        DebugLog_Printf("JUMP",
            "SENT targetTick=%u cmdTick=%u buttons=0x%02X JumpPending=%d",
            m_TargetTick, m_LastCmd.tickId, m_LastCmd.buttons, (int)m_JumpPending);
    }

    // 4. Clear sticky jump only when server confirms we're airborne
    //    This ensures jump isn't lost due to frame/tick timing
    if (m_JumpPending && m_HasServerState)
    {
        // Clear pending if server shows we're jumping (not grounded)
        if ((m_LastServerState.stateFlags & NetStateFlags::IS_JUMPING) ||
            !(m_LastServerState.stateFlags & NetStateFlags::IS_GROUNDED))
        {
            m_JumpPending = false;
            if (DebugLog_IsEnabled() && DebugLog_LogJumpEvents())
            {
                DebugLog_Printf("JUMP",
                    "CONSUMED_BY_SERVER targetTick=%u serverFlags=0x%X",
                    m_TargetTick, m_LastServerState.stateFlags);
            }
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
    // Camera Angles (from PlayerCamFps) — always sampled so a paused player's
    // aim direction stays where they left it (camera doesn't move while paused).
    // ========================================================================
    DirectX::XMFLOAT3 camFront = PlayerCamFps_GetFront();
    m_Yaw   = atan2f(camFront.x, camFront.z);
    m_Pitch = asinf(camFront.y);

    // ========================================================================
    // Zero movement/button intent when gameplay isn't active (paused, in a
    // menu, or in a non-game scene). Tick still advances and camera angle is
    // still reported, but the player neither moves nor fires. (docs §5.4)
    // ========================================================================
    m_MoveAxisX = 0.0f;
    m_MoveAxisY = 0.0f;
    m_Buttons   = InputButtons::NONE;

    if (!Game_IsGameplayActive())
    {
        m_JumpPending = false;  // drop any sticky jump captured before pausing
        return;
    }

    // ========================================================================
    // Movement Axis (WASD)
    // ========================================================================
    if (KeyLogger_IsPressed(KK_W)) m_MoveAxisY += 1.0f;
    if (KeyLogger_IsPressed(KK_S)) m_MoveAxisY -= 1.0f;
    if (KeyLogger_IsPressed(KK_D)) m_MoveAxisX += 1.0f;
    if (KeyLogger_IsPressed(KK_A)) m_MoveAxisX -= 1.0f;

    // JUMP: Sticky - capture trigger and keep pending until consumed
    if (KeyLogger_IsTrigger(KK_SPACE))
    {
        m_JumpPending = true;
        if (DebugLog_IsEnabled() && DebugLog_LogJumpEvents())
        {
            DebugLog_Printf("JUMP", "SPACE_TRIG targetTick=%u", m_TargetTick);
        }
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
    InputCmd cmd{};
    // Stamp with player_fps's m_CurrentClientTick so the cmd.tickId we send is
    // in the same domain as PlayerFps::m_InputHistory.cmd.tickId. The server
    // echoes this value back as NetPlayerState.lastProcessedInputTick so the
    // client can find the matching history entry for RESIM.
    cmd.tickId = Game_GetClientTick();
    cmd.moveAxisX = m_MoveAxisX;
    cmd.moveAxisY = m_MoveAxisY;
    cmd.yaw = m_Yaw;
    cmd.pitch = m_Pitch;
    cmd.buttons = m_Buttons;
    // Lag compensation: report which (fractional) server tick we are viewing
    // (remote players render interp-delayed). The server rewinds hitboxes to
    // this time when this cmd fires. viewTick == 0 → no data, no rewind.
    Game_GetViewTick(cmd.viewTick, cmd.viewTickFrac);
    return cmd;
}
