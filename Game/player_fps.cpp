#include "player_fps.h"
#include "player_cam_fps.h"
#include "animator.h"
#include "key_logger.h"
#include "cube.h"
#include "debug_log.h"
#include "direct3d.h"
#include "fade.h"
#include "game.h"
#include "input_producer.h"
#include "mouse.h"
#include "ms_logger.h"
#include "shader_3d_ani.h"

using namespace DirectX;

PlayerFps::PlayerFps()
	: m_Position({ 0,0,0 })
	, m_Velocity({ 0,0,0 })
	, m_RenderOffset({ 0,0,0 })
	, m_CorrectionMode("NONE")
	, m_CorrectionError(0.0f)
	, m_LastServerTick(0)
	, m_InputHistoryHead(0)
	, m_InputHistoryCount(0)
	, m_CurrentClientTick(0)
	, m_InResimulation(false)
	, m_ModelFront({ 0,0,1 })
	, m_CamRelativePos({ 0.0f, 0.0f,0.3f })
	, m_Height(1.6f)
	, m_CapsuleRadius(0.3f)
	, m_IsJump(false)
	, m_JumpPending(false)
	, m_pCollisionWorld(nullptr)
	, m_PhysicsAccumulator(0.0)
	, m_PrevPhysicsPosition({ 0,0,0 })
	, m_PhysicsAlpha(0.0f)
	, m_Model(nullptr)
	, m_Animator(nullptr)
	, m_StateMachine(nullptr)
	, m_Ammo(WeaponConfig::MAG_SIZE)
	, m_AmmoReserve(WeaponConfig::MAX_RESERVE)
	, m_WeaponRPM(600.0)
	, m_FireTimer(0.0)
	, m_FireCounter(0)
	, m_TransitionFiring(false)
	, m_TeamId(PlayerTeam::RED)
	, m_Health(200)
	, m_IsDead(false)
	, m_WasDead(true)   // treat first connection as respawn so yaw is initialized
{
}

PlayerFps::~PlayerFps()
{
	Finalize();
}

void PlayerFps::Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front,
                            CollisionWorld* pCollisionWorld)
{
	m_Position = position;
	m_pCollisionWorld = pCollisionWorld;
	m_Velocity = { 0.0f, 0.0f, 0.0f };
	m_RenderOffset = { 0.0f, 0.0f, 0.0f };
	m_CorrectionMode = "NONE";
	m_CorrectionError = 0.0f;
	m_LastServerTick = 0;
	m_PhysicsAccumulator = 0.0;
	m_PrevPhysicsPosition = position;
	m_PhysicsAlpha = 0.0f;
	m_WasDead = true;   // first snapshot will set yaw toward world center
	m_Ammo        = WeaponConfig::MAG_SIZE;
	m_AmmoReserve = WeaponConfig::MAX_RESERVE;

	XMStoreFloat3(&m_ModelFront, XMVector3Normalize(XMLoadFloat3(&front)));

	m_StateMachine = new PlayerStateMachine();
	
	const char* modelPath = (m_TeamId == PlayerTeam::BLUE)
		? "resource/model/blue_arm003.fbx"
		: "resource/model/red_arm003.fbx";
	m_Model = ModelAni_Load(modelPath);

	if (m_Model)
	{
		m_Animator = new Animator();
		m_Animator->Init(m_Model);
	}
}

void PlayerFps::Finalize()
{
	if (m_Animator)
	{
		delete m_Animator;
		m_Animator = nullptr;
	}
	if (m_Model)
	{
		ModelAni_Release(m_Model);
		m_Model = nullptr;
	}
}

void PlayerFps::SetTeam(uint8_t teamId)
{
	if (teamId == m_TeamId) return;
	m_TeamId = teamId;

	// Swap model for new team
	if (m_Animator)
	{
		delete m_Animator;
		m_Animator = nullptr;
	}
	if (m_Model)
	{
		ModelAni_Release(m_Model);
		m_Model = nullptr;
	}

	const char* modelPath = (m_TeamId == PlayerTeam::BLUE)
		? "resource/model/blue_arm003.fbx"
		: "resource/model/red_arm003.fbx";
	m_Model = ModelAni_Load(modelPath);

	if (m_Model)
	{
		m_Animator = new Animator();
		m_Animator->Init(m_Model);
	}
}

void PlayerFps::Update(double elapsed_time)
{
	const float frameDt = static_cast<float>(elapsed_time);

	// ========================================================================
	// Render Offset Decay (for smooth server correction) — runs at FRAME RATE
	// Decays visual offset toward zero over ~100ms
	// ========================================================================
	constexpr float OFFSET_DECAY_RATE = 20.0f;  // Faster decay = less visible correction
	float decayFactor = 1.0f - OFFSET_DECAY_RATE * frameDt;
	if (decayFactor < 0.0f) decayFactor = 0.0f;
	m_RenderOffset.x *= decayFactor;
	m_RenderOffset.y *= decayFactor;
	m_RenderOffset.z *= decayFactor;

	// ========================================================================
	// Dead — skip all input and gameplay
	// ========================================================================
	if (m_IsDead)
	{
		return;
	}

	// ========================================================================
	// FIXED-TIMESTEP PHYSICS (must match server 32Hz tick rate)
	// Accumulator pattern: step physics at exactly TICK_DURATION intervals
	// This eliminates prediction divergence from frame-rate dependent dt
	// ========================================================================
	const double maxDelta = TICK_DURATION * 4.0;  // Max 4 ticks per frame
	double clampedDelta = (elapsed_time > maxDelta) ? maxDelta : elapsed_time;
	m_PhysicsAccumulator += clampedDelta;

	// Sample input from InputProducer (already converted to InputCmd)
	InputCmd currentCmd;
	if (g_pInputProducer)
	{
		currentCmd = g_pInputProducer->GetLastInputCmd();
	}
	else
	{
		// Fallback: create empty command if no InputProducer
		currentCmd = {};
		currentCmd.tickId = m_CurrentClientTick;
	}

	// Update camera model front
	XMFLOAT3 camFront = PlayerCamFps_GetFront();
	m_ModelFront = camFront;

	// Convert InputCmd movement axes to world space
	// CRITICAL: Must match server's method exactly (game_server.cpp:322-329)
	// Server uses yaw only (2D), not 3D camera vector projection
	float yaw = currentCmd.yaw;
	float frontX = sinf(yaw);
	float frontZ = cosf(yaw);
	float rightX = frontZ;
	float rightZ = -frontX;

	// Transform camera-relative input to world space
	float worldInputX = currentCmd.moveAxisX * rightX + currentCmd.moveAxisY * frontX;
	float worldInputZ = currentCmd.moveAxisX * rightZ + currentCmd.moveAxisY * frontZ;

	// Normalize if magnitude > 1.0
	float inputMag = sqrtf(worldInputX * worldInputX + worldInputZ * worldInputZ);
	if (inputMag > 1.0f) { worldInputX /= inputMag; worldInputZ /= inputMag; }

	// Extract other input flags for animation and state machine (used after physics loop)
	bool tryRunning = (currentCmd.buttons & InputButtons::SPRINT) != 0;

	// Sample jump input (still using keyboard for frame-rate independent capture)
	if (KeyLogger_IsTrigger(KK_SPACE)) m_JumpPending = true;

	// ========================================================================
	// PHYSICS TICK LOOP with Input History Recording
	// ========================================================================
	while (m_PhysicsAccumulator >= TICK_DURATION)
	{
		// Save position before this tick (for sub-tick interpolation)
		m_PrevPhysicsPosition = m_Position;
		const float dt = static_cast<float>(TICK_DURATION);

		// Increment client tick (sync with server tick on first snapshot)
		m_CurrentClientTick++;

		// Update InputCmd tickId for this physics tick
		InputCmd tickCmd = currentCmd;
		tickCmd.tickId = m_CurrentClientTick;

		// Apply physics simulation with current input
		ApplyPhysicsTick(worldInputX, worldInputZ, tickCmd.buttons, dt);

		// Record input + resulting state in history buffer
		RecordInputHistory(tickCmd, worldInputX, worldInputZ);

		m_PhysicsAccumulator -= TICK_DURATION;
	}

	// Sub-tick interpolation alpha (0.0 = at last tick, 1.0 = at next tick)
	m_PhysicsAlpha = static_cast<float>(m_PhysicsAccumulator / TICK_DURATION);

	// ========================================================================
	// Update Player State for Animations — runs at FRAME RATE
	// ========================================================================
	if (inputMag > 0.01f)
	{
		m_StateMachine->SetPlayerState(tryRunning ? PlayerState::RUNNING : PlayerState::WALKING);
	}
	else
	{
		m_StateMachine->SetPlayerState(PlayerState::IDLE);
	}

	// ========================================================================
	// Weapon State Machine (unchanged)
	// ========================================================================
	bool isPressingLeft = isButtonDown(MBT_LEFT);
	bool isPressingRight = isButtonDown(MBT_RIGHT);
	
	if (isPressingRight &&
		m_StateMachine->GetWeaponState() != WeaponState::ADS &&
		m_StateMachine->GetWeaponState() != WeaponState::ADS_FIRING &&
		m_StateMachine->GetWeaponState() != WeaponState::RELOADING &&
		m_StateMachine->GetWeaponState() != WeaponState::RELOADING_OUT_OF_AMMO)
	{
		m_StateMachine->SetWeaponState(WeaponState::ADS_IN);
	}

	if (!isPressingRight && 
		(m_StateMachine->GetWeaponState() == WeaponState::ADS || 
		 m_StateMachine->GetWeaponState() == WeaponState::ADS_IN ||
		 m_StateMachine->GetWeaponState() == WeaponState::ADS_FIRING))
	{
		// Exit ADS — transition fire (additive) will keep firing if left is held
		m_StateMachine->SetWeaponState(WeaponState::ADS_OUT);
	}

	// ---- TRANSITION FIRE: fire during ADS_IN / ADS_OUT using additive ----
	{
		WeaponState ws = m_StateMachine->GetWeaponState();
		bool inTransition = (ws == WeaponState::ADS_IN || ws == WeaponState::ADS_OUT);

		if (inTransition && isPressingLeft)
		{
			if (!m_TransitionFiring) {
				// First shot
				m_TransitionFiring = true;
				m_FireTimer = 0.0;
				if (m_Ammo > 0) { m_Ammo--; m_FireCounter++; }
			} else {
				// Full-auto at RPM interval
				m_FireTimer += frameDt;
				const double fireInterval = 60.0 / m_WeaponRPM;
				if (m_FireTimer >= fireInterval) {
					m_FireTimer -= fireInterval;
					if (m_Ammo > 0) { m_Ammo--; m_FireCounter++; }
				}
			}
		}
		else
		{
			m_TransitionFiring = false;
		}
	}

	// ---- ADS FIRE: click to start, hold for full-auto ----
	if (isPressingLeft && m_StateMachine->GetWeaponState() == WeaponState::ADS)
	{
		if (m_Ammo > 0) {
			// First shot on press
			m_StateMachine->SetWeaponState(WeaponState::ADS_FIRING);
			m_FireTimer = 0.0;
			m_Ammo--; m_FireCounter++;
		} else if (m_AmmoReserve > 0) {
			m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
		}
	}

	if (!isPressingLeft && m_StateMachine->GetWeaponState() == WeaponState::ADS_FIRING)
	{
		m_StateMachine->SetWeaponState(WeaponState::ADS);
	}

	if (m_StateMachine->GetWeaponState() == WeaponState::ADS_FIRING)
	{
		if (isPressingLeft) {
			// Full-auto: accumulate timer and fire at RPM interval
			m_FireTimer += frameDt;
			const double fireInterval = 60.0 / m_WeaponRPM;
			if (m_FireTimer >= fireInterval) {
				m_FireTimer -= fireInterval;
				m_Animator->SetSameAniOverlapAllow(true);
				if (m_Ammo > 0) { m_Ammo--; m_FireCounter++; }
				else if (m_AmmoReserve > 0) {
					m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
				}
			}
		}
	}

	// ---- HIP FIRE: click to start, hold for full-auto ----
	if (isPressingLeft && m_StateMachine->GetWeaponState() == WeaponState::HIP)
	{
		if (m_Ammo > 0) {
			// First shot on press
			m_StateMachine->SetWeaponState(WeaponState::HIP_FIRING);
			m_FireTimer = 0.0;
			m_Ammo--; m_FireCounter++;
		} else if (m_AmmoReserve > 0) {
			m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
		}
	}

	if (m_StateMachine->GetWeaponState() == WeaponState::HIP_FIRING)
	{
		if (isPressingLeft) {
			// Full-auto: accumulate timer and fire at RPM interval
			m_FireTimer += frameDt;
			const double fireInterval = 60.0 / m_WeaponRPM;
			if (m_FireTimer >= fireInterval) {
				m_FireTimer -= fireInterval;
				m_Animator->SetSameAniOverlapAllow(true);
				if (m_Ammo > 0) { m_Ammo--; m_FireCounter++; }
				else if (m_AmmoReserve > 0) {
					m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
				}
			}
		}
	}

	if (KeyLogger_IsTrigger(KK_R))
	{
		WeaponState rws = m_StateMachine->GetWeaponState();
		bool alreadyReloading = (rws == WeaponState::RELOADING ||
		                         rws == WeaponState::RELOADING_OUT_OF_AMMO);
		if (!alreadyReloading && m_Ammo < WeaponConfig::MAG_SIZE && m_AmmoReserve > 0)
		{
			WeaponState nextReload = (m_Ammo == 0)
				? WeaponState::RELOADING_OUT_OF_AMMO
				: WeaponState::RELOADING;
			m_StateMachine->SetWeaponState(nextReload);
		}
	}

	if (KeyLogger_IsTrigger(KK_E))
	{
		m_StateMachine->SetWeaponState(WeaponState::INSPECTING);
	}

	// INSPECTING exits on any movement or action input
	if (m_StateMachine->GetWeaponState() == WeaponState::INSPECTING)
	{
		bool anyInput = isPressingLeft || isPressingRight ||
		                KeyLogger_IsPressed(KK_W) || KeyLogger_IsPressed(KK_A) ||
		                KeyLogger_IsPressed(KK_S) || KeyLogger_IsPressed(KK_D) ||
		                KeyLogger_IsTrigger(KK_R);
		if (anyInput)
			m_StateMachine->SetWeaponState(WeaponState::HIP);
	}

	// Update Model Animation
	if (m_Model && m_Animator)
	{
		WeaponState prevWs = m_StateMachine->GetWeaponState();
		m_StateMachine->Update(elapsed_time, m_Animator);
		WeaponState curWs = m_StateMachine->GetWeaponState();

		// Reload complete: refill magazine from reserve
		if (curWs == WeaponState::HIP &&
		    (prevWs == WeaponState::RELOADING ||
		     prevWs == WeaponState::RELOADING_OUT_OF_AMMO))
		{
			int needed    = WeaponConfig::MAG_SIZE - m_Ammo;
			int refill    = (m_AmmoReserve >= needed) ? needed : m_AmmoReserve;
			m_Ammo        += refill;
			m_AmmoReserve -= refill;
		}

		// Override additive layer: fire animation during ADS transition
		if (m_TransitionFiring)
		{
			// PlayAdditive with fire animation (index 4), non-loop, full weight
			m_Animator->PlayAdditive(4, false, 1.0f);
		}
	}

}

void PlayerFps::Draw()
{
	if (!m_Model) return;

	// Calculate World Matrix for Arms (Attached to Camera)
	// We want to construct a basis at CameraPosition + RelativeOffset
	
	XMFLOAT3 camPos = PlayerCamFps_GetPosition(); // Should be Camera position
	XMFLOAT3 camFront3 = PlayerCamFps_GetFront();
	
	XMVECTOR cPos = XMLoadFloat3(&camPos);
	XMVECTOR cFront = XMVector3Normalize(XMLoadFloat3(&camFront3));
	XMVECTOR cUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	
	// Create Camera Basis
	XMVECTOR cRight = XMVector3Normalize(XMVector3Cross(cUp, cFront));
	// Recalculate Up to be orthogonal
	cUp = XMVector3Cross(cFront, cRight);

	// Calculate Model Position in World Space
	// Pos = CamPos + Right*Rel.x + Up*Rel.y + Front*Rel.z
	XMVECTOR modelPos = cPos 
		+ cRight * m_CamRelativePos.x 
		+ cUp * m_CamRelativePos.y 
		+ cFront * m_CamRelativePos.z;

	// Rotation: The model needs to rotate to face cFront.
	// Assuming default model forward is +Z.
	

	XMMATRIX world = XMMatrixIdentity();
	world = XMMatrixRotationX(XMConvertToRadians(-90.0f)) * world; // Rotate -90 degrees around X to align model's up with world's up

	world.r[0] = cRight;
	world.r[1] = cUp;
	world.r[2] = cFront;
	world.r[3] = XMVectorSet(0,0,0,1); 

	// Translation
	world.r[3] = XMVectorSetW(modelPos, 1.0f);

	world = XMMatrixRotationY(XMConvertToRadians(180.0f))  * world; // Rotate 180 degrees around Y to face camera
	world = XMMatrixTranslation(0.0f, -1.085f, 0.0f) * world; // Adjust vertical position if needed

	ModelAni_Draw(m_Model, world, m_Animator, true); // isBlender=false as we constructed the matrix manually
}

const DirectX::XMFLOAT3& PlayerFps::GetPosition() const
{
	return m_Position;
}

const DirectX::XMFLOAT3& PlayerFps::GetFront() const
{
	return m_ModelFront;
}

//-----------------------------------------------------------------------------
// GetRenderPosition - Sub-tick interpolated position + server correction offset
//   Lerp between previous and current physics position using accumulator remainder
//   Then add server correction render offset on top
//-----------------------------------------------------------------------------
DirectX::XMFLOAT3 PlayerFps::GetRenderPosition() const
{
	float a = m_PhysicsAlpha;
	return {
		m_PrevPhysicsPosition.x + (m_Position.x - m_PrevPhysicsPosition.x) * a + m_RenderOffset.x,
		m_PrevPhysicsPosition.y + (m_Position.y - m_PrevPhysicsPosition.y) * a + m_RenderOffset.y,
		m_PrevPhysicsPosition.z + (m_Position.z - m_PrevPhysicsPosition.z) * a + m_RenderOffset.z
	};
}

//-----------------------------------------------------------------------------
// ApplyServerCorrection - Prediction + Correction (Server Reconciliation)
//
// Called when server snapshot is received.
// Soft Correction: Small error -> apply visual offset, decay over time
// Hard Snap: Large error -> teleport immediately
//-----------------------------------------------------------------------------
void PlayerFps::ApplyServerCorrection(const NetPlayerState& serverState)
{
	// Skip if same tick already processed
	if (serverState.tickId <= m_LastServerTick && m_LastServerTick != 0)
		return;

	// Capture mode at entry — used for MODE_CHANGE log at the end of this function.
	const char* prevMode = m_CorrectionMode ? m_CorrectionMode : "NONE";

	// SOFT_PERSIST periodic sample: while we are stuck in SOFT, log state every N server ticks
	// so we can see whether error / renderOffset is converging or runaway.
	if (DebugLog_IsEnabled() && DebugLog_LogSoftModeState() &&
		m_CorrectionMode && strcmp(m_CorrectionMode, "SOFT") == 0)
	{
		const int interval = DebugLog_GetSoftStateIntervalTicks();
		if (interval > 0 && (serverState.tickId % static_cast<uint32_t>(interval)) == 0)
		{
			float pdx = m_Position.x - serverState.position.x;
			float pdy = m_Position.y - serverState.position.y;
			float pdz = m_Position.z - serverState.position.z;
			float perr = sqrtf(pdx * pdx + pdy * pdy + pdz * pdz);
			DebugLog_Printf("CORR",
				"SOFT_PERSIST cTick=%u sTick=%u err=%.3fm renderOffset=(%.2f,%.2f,%.2f) "
				"clientIsJump=%d serverGrounded=%d",
				m_CurrentClientTick, serverState.tickId, perr,
				m_RenderOffset.x, m_RenderOffset.y, m_RenderOffset.z,
				(int)m_IsJump,
				(int)((serverState.stateFlags & NetStateFlags::IS_GROUNDED) != 0));
		}
	}

	// Initialize client tick from first server snapshot
	if (m_CurrentClientTick == 0)
	{
		m_CurrentClientTick = serverState.tickId;
		// Any history entries built before the first server snapshot used the
		// pre-sync local tick domain (counted up from 0). Server's
		// lastProcessedInputTick acks will be in the new (server-aligned) domain
		// after sync, so the old entries can never match — drop them to avoid
		// stale FindHistoryEntry hits.
		ClearInputHistory();
	}
	else
	{
		// Detect tick drift and force resync if too large
		// This can happen due to frame rate variance or packet loss
		int tickDrift = static_cast<int>(m_CurrentClientTick) - static_cast<int>(serverState.tickId);

		// Expected drift: client is ahead by RTT/2 (~1-3 ticks for 30-100ms RTT)
		// If drift is too large (>10 ticks = 312ms), force resync
		if (tickDrift > 10 || tickDrift < -5)
		{
			DebugLog_Printf("CORR",
				"TICK_DRIFT_RESET drift=%d cTick=%u sTick=%u (clearing history)",
				tickDrift, m_CurrentClientTick, serverState.tickId);
			// Abnormal drift detected - resync to server
			m_CurrentClientTick = serverState.tickId;
			ClearInputHistory();  // History is no longer valid
		}
	}

	m_LastServerTick = serverState.tickId;

	// Correction thresholds with hysteresis to prevent rapid switching
	// RESIM: Re-simulate when error exceeds threshold
	// HARD: Teleport for true divergence (collision bug, respawn, teleport)
	// Use lower threshold if already correcting to avoid flickering between OK/RESIM
	const bool wasCorrect = (m_CorrectionMode == nullptr || strcmp(m_CorrectionMode, "OK") == 0);
	const float RESIM_THRESHOLD = wasCorrect ? 0.25f : 0.1f;  // Higher to enter, lower to stay
	constexpr float HARD_SNAP_THRESHOLD = 4.0f;                // Full teleport

	// Calculate error between predicted and server position
	float dx = m_Position.x - serverState.position.x;
	float dy = m_Position.y - serverState.position.y;
	float dz = m_Position.z - serverState.position.z;
	float error = sqrtf(dx * dx + dy * dy + dz * dz);

	m_CorrectionError = error;

	if (error > HARD_SNAP_THRESHOLD)
	{
		// ===== HARD SNAP =====
		// True divergence (bug / respawn): teleport immediately
		m_CorrectionMode = "SNAP";
		m_Position = serverState.position;
		m_PrevPhysicsPosition = serverState.position;
		m_Velocity = serverState.velocity;
		m_RenderOffset = { 0.0f, 0.0f, 0.0f };
		m_IsJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

		// Clear input history on hard snap
		ClearInputHistory();
	}
	else if (error > RESIM_THRESHOLD)
	{
		// ===== RE-SIMULATION =====
		// Look up the history entry for the cmd the server most-recently processed.
		// (serverState.tickId is the server's own tick counter; serverState.lastProcessedInputTick
		//  is in the same domain as our InputHistoryEntry.cmd.tickId — the only one that can match.)
		const uint32_t ackTick = serverState.lastProcessedInputTick;
		InputHistoryEntry* serverEntry = (ackTick != 0) ? FindHistoryEntry(ackTick) : nullptr;

		if (!serverEntry)
		{
			// Compute history range for diagnostics — captured BEFORE fallback so the
			// log row reflects the buffer state at the moment FindHistoryEntry failed.
			uint32_t oldestTick = 0, newestTick = 0;
			if (m_InputHistoryCount > 0)
			{
				oldestTick = newestTick = m_InputHistory[0].cmd.tickId;
				for (int i = 1; i < m_InputHistoryCount; ++i)
				{
					uint32_t t = m_InputHistory[i].cmd.tickId;
					if (t < oldestTick) oldestTick = t;
					if (t > newestTick) newestTick = t;
				}
			}
			DebugLog_Printf("CORR",
				"SOFT_FALLBACK cTick=%u sTick=%u err=%.3fm dx=%.2f dy=%.2f dz=%.2f "
				"histCount=%d histRange=[%u..%u] requestedTick=%u "
				"renderOffsetBefore=(%.2f,%.2f,%.2f) clientIsJump=%d serverGrounded=%d",
				m_CurrentClientTick, serverState.tickId, error, dx, dy, dz,
				m_InputHistoryCount, oldestTick, newestTick, ackTick,
				m_RenderOffset.x, m_RenderOffset.y, m_RenderOffset.z,
				(int)m_IsJump,
				(int)((serverState.stateFlags & NetStateFlags::IS_GROUNDED) != 0));

			// History miss (cmd too old, dropped, or first-snapshot edge case):
			// align to server, smooth via render-offset decay. Same pattern as RESIM tail
			// (see below) — without the resim step, since we have no anchor entry.
			DirectX::XMFLOAT3 originalPredictedPos = m_Position;
			m_Position    = serverState.position;
			m_Velocity    = serverState.velocity;
			m_IsJump      = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);
			// = (not +=) so RenderOffset doesn't unbounded-accumulate across consecutive misses.
			m_RenderOffset.x = originalPredictedPos.x - m_Position.x;
			m_RenderOffset.y = originalPredictedPos.y - m_Position.y;
			m_RenderOffset.z = originalPredictedPos.z - m_Position.z;
			m_CorrectionMode = "SOFT";  // keep the literal — debug_log filters key off it
		}
		else
		{
			// Save original predicted position for visual offset calculation
			DirectX::XMFLOAT3 originalPredictedPos = m_Position;

			// Replace history entry with server authoritative state
			serverEntry->position = serverState.position;
			serverEntry->velocity = serverState.velocity;
			serverEntry->stateFlags = serverState.stateFlags;

			// Reset current physics state to server state
			m_Position = serverState.position;
			m_Velocity = serverState.velocity;
			m_IsJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

			// Re-simulate all ticks from acked cmd tick to current tick
			ResimulateFromTick(ackTick);

			// Verify re-simulation quality: check that re-sim matches server at the ack tick
			InputHistoryEntry* verifyEntry = FindHistoryEntry(ackTick);
			if (verifyEntry)
			{
				float vdx = verifyEntry->position.x - serverState.position.x;
				float vdy = verifyEntry->position.y - serverState.position.y;
				float vdz = verifyEntry->position.z - serverState.position.z;
				float verifyError = sqrtf(vdx * vdx + vdy * vdy + vdz * vdz);

				// Update displayed error to show post-resim accuracy
				// (Should be near-zero if physics is deterministic)
				m_CorrectionError = verifyError;
			}

			// Calculate visual offset for smooth transition
			// (originalPos - correctedPos) makes render position stay at originalPos initially
			m_RenderOffset.x = originalPredictedPos.x - m_Position.x;
			m_RenderOffset.y = originalPredictedPos.y - m_Position.y;
			m_RenderOffset.z = originalPredictedPos.z - m_Position.z;

			m_CorrectionMode = "RESIM";
		}
	}
	else
	{
		// ===== NO CORRECTION =====
		// Prediction is accurate (<0.1m error)
		m_CorrectionMode = "OK";
	}

	// ------------------------------------------------------------------------
	// Diagnostic logging: mode transitions, threshold-based correction rows,
	// and client/server grounded-state mismatch.
	// ------------------------------------------------------------------------
	if (DebugLog_IsEnabled())
	{
		const char* nowMode = m_CorrectionMode ? m_CorrectionMode : "NONE";
		bool serverGrounded = (serverState.stateFlags & NetStateFlags::IS_GROUNDED) != 0;
		bool clientAirborne = m_IsJump;

		if (strcmp(prevMode, nowMode) != 0)
		{
			DebugLog_Printf("CORR",
				"MODE_CHANGE %s->%s cTick=%u sTick=%u err=%.3fm",
				prevMode, nowMode, m_CurrentClientTick, serverState.tickId, error);
		}

		if (DebugLog_LogEveryCorrection() || error > DebugLog_GetErrorThreshold())
		{
			DebugLog_Printf("CORR",
				"%s cTick=%u sTick=%u err=%.3fm dx=%.2f dy=%.2f dz=%.2f "
				"renderOffset=(%.2f,%.2f,%.2f) clientIsJump=%d serverGrounded=%d",
				nowMode, m_CurrentClientTick, serverState.tickId, error, dx, dy, dz,
				m_RenderOffset.x, m_RenderOffset.y, m_RenderOffset.z,
				(int)clientAirborne, (int)serverGrounded);
		}

		// Grounded-state mismatch is the primary suspect for "jump fails / lags".
		// Local prediction needs !m_IsJump (grounded) to apply jump impulse;
		// server needs IS_GROUNDED to accept JUMP bit.
		if (clientAirborne == serverGrounded)
		{
			DebugLog_Printf("CORR",
				"STATE_MISMATCH client_isJump=%d server_grounded=%d mode=%s err=%.3fm sTick=%u",
				(int)clientAirborne, (int)serverGrounded, nowMode, error, serverState.tickId);
		}
	}

	// ========================================================================
	// Combat State: health and death
	// ========================================================================
	m_Health = serverState.health;
	bool isDead = (serverState.stateFlags & NetStateFlags::IS_DEAD) != 0;

	if (isDead && !m_WasDead)
	{
		// Just died — fade to red
		Fade_Start(0.5, true, { 0.5f, 0.0f, 0.0f });
		m_IsDead = true;

		// Clear input history on death
		ClearInputHistory();
	}
	else if (!isDead && m_WasDead)
	{
		// Just respawned — fade in from black, snap to spawn position
		Fade_Start(0.5, false, { 0.0f, 0.0f, 0.0f });
		m_IsDead = false;
		m_Position = serverState.position;
		m_PrevPhysicsPosition = serverState.position;
		m_Velocity = serverState.velocity;
		m_RenderOffset = { 0.0f, 0.0f, 0.0f };
		m_IsJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

		// Face toward world center (0, 0, 0)
		float dx = 0.0f - serverState.position.x;
		float dz = 0.0f - serverState.position.z;
		float yaw = atan2f(dx, dz);
		PlayerCamFps_SetYaw(yaw);
		PlayerCamFps_SetPitch(0.0f);

		// Weapon state reset on respawn
		m_StateMachine->SetWeaponState(WeaponState::HIP);

		// Clear input history and sync tick on respawn
		ClearInputHistory();
		m_CurrentClientTick = serverState.tickId;
	}
	bool wasDeadBefore = m_WasDead;
	m_WasDead = isDead;

	// Sync authoritative ammo only on respawn or large divergence
	// (avoids HUD flicker from RTT delay during continuous firing)
	static constexpr int AMMO_SYNC_THRESHOLD = 2;
	int ammoDiff = abs(static_cast<int>(serverState.ammo) - m_Ammo);
	int reserveDiff = abs(static_cast<int>(serverState.ammoReserve) - m_AmmoReserve);
	if ((!isDead && wasDeadBefore) || ammoDiff > AMMO_SYNC_THRESHOLD || reserveDiff > AMMO_SYNC_THRESHOLD)
	{
		m_Ammo = serverState.ammo;
		m_AmmoReserve = serverState.ammoReserve;
	}
}

AABB PlayerFps::GetAABB() const
{
	return {
		{m_Position.x - 1.0f, m_Position.y, m_Position.z - 1.0f },
		{m_Position.x + 1.0f, m_Position.y + m_Height, m_Position.z + 1.0f}
	};
}

Capsule PlayerFps::GetCapsule() const
{
	return {
		{ m_Position.x, m_Position.y + m_CapsuleRadius, m_Position.z },
		{ m_Position.x, m_Position.y + m_Height - m_CapsuleRadius, m_Position.z },
		m_CapsuleRadius
	};
}

DirectX::XMFLOAT3 PlayerFps::GetEyePosition() const
{
	// Use interpolated position for smooth camera
	float a = m_PhysicsAlpha;
	DirectX::XMFLOAT3 eyePos;
	eyePos.x = m_PrevPhysicsPosition.x + (m_Position.x - m_PrevPhysicsPosition.x) * a;
	eyePos.y = m_PrevPhysicsPosition.y + (m_Position.y - m_PrevPhysicsPosition.y) * a + 1.5f;
	eyePos.z = m_PrevPhysicsPosition.z + (m_Position.z - m_PrevPhysicsPosition.z) * a;
	return eyePos;
}

std::string PlayerFps::GetPlayerState() const
{
	return PlayerStateToString(m_StateMachine->GetPlayerState());
}

std::string PlayerFps::GetWeaponState() const
{
	return WeaponStateToString(m_StateMachine->GetWeaponState());
}

std::string PlayerFps::GetCurrentAniName() const
{
	if (m_Model && m_Animator)
	{
		int aniIndex = m_Animator->GetCurrentAnimationIndex();
		if (aniIndex >= 0 && aniIndex < static_cast<int>(m_Model->Animations.size()))
		{
			return m_Model->Animations[aniIndex].name;
		}
	}
	return "No Animation";
}

float PlayerFps::GetCurrentAniDuration() const
{
	if (m_Model && m_Animator)
	{
		int aniIndex = m_Animator->GetCurrentAnimationIndex();
		if (aniIndex >= 0 && aniIndex < static_cast<int>(m_Model->Animations.size()))
		{
			return static_cast<float>(m_Model->Animations[aniIndex].duration);
		}
	}
	return 0.0f;
}

float PlayerFps::GetCurrentAniProgress() const
{
	if (m_Animator)
	{
		return m_Animator->GetCurrAniProgress();
	}
	return 0.0f;
}

//=============================================================================
// Client-Side Prediction Reconciliation - Input History & Re-simulation
//=============================================================================

void PlayerFps::RecordInputHistory(const InputCmd& cmd, float worldInputX, float worldInputZ)
{
	// Store input + resulting physics state in circular buffer
	InputHistoryEntry& entry = m_InputHistory[m_InputHistoryHead];
	entry.cmd = cmd;
	entry.worldInputX = worldInputX;
	entry.worldInputZ = worldInputZ;
	entry.position = m_Position;
	entry.velocity = m_Velocity;
	entry.stateFlags = GetStateFlags();

	// Advance circular buffer head
	m_InputHistoryHead = (m_InputHistoryHead + 1) % INPUT_HISTORY_SIZE;

	// Track count (max INPUT_HISTORY_SIZE)
	if (m_InputHistoryCount < INPUT_HISTORY_SIZE)
		m_InputHistoryCount++;
}

InputHistoryEntry* PlayerFps::FindHistoryEntry(uint32_t tickId)
{
	// Linear search through circular buffer (only 10 entries, fast enough)
	for (int i = 0; i < m_InputHistoryCount; i++)
	{
		int index = (m_InputHistoryHead - 1 - i + INPUT_HISTORY_SIZE) % INPUT_HISTORY_SIZE;
		if (m_InputHistory[index].cmd.tickId == tickId)
		{
			return &m_InputHistory[index];
		}
	}
	return nullptr;  // Tick not found (too old or not recorded)
}

void PlayerFps::ClearInputHistory()
{
	m_InputHistoryHead = 0;
	m_InputHistoryCount = 0;
}

uint32_t PlayerFps::GetStateFlags() const
{
	uint32_t flags = 0;
	if (m_IsJump)
		flags |= NetStateFlags::IS_JUMPING;
	if (!m_IsJump)
		flags |= NetStateFlags::IS_GROUNDED;
	// Add other state flags as needed (IS_FIRING, IS_ADS, etc.)
	return flags;
}

void PlayerFps::ResimulateFromTick(uint32_t serverTick)
{
	// Replay all ticks from serverTick+1 to m_CurrentClientTick
	// This corrects client prediction based on server's authoritative state
	const float dt = static_cast<float>(TICK_DURATION);

	m_InResimulation = true;
	for (uint32_t tick = serverTick + 1; tick <= m_CurrentClientTick; tick++)
	{
		InputHistoryEntry* entry = FindHistoryEntry(tick);
		if (!entry)
		{
			// Tick not found in history (too old or missing)
			// This shouldn't happen if buffer size is adequate
			break;
		}

		// Re-apply physics with the same input that was used originally
		ApplyPhysicsTick(entry->worldInputX, entry->worldInputZ, entry->cmd.buttons, dt);

		// Update history entry with corrected result
		entry->position = m_Position;
		entry->velocity = m_Velocity;
		entry->stateFlags = GetStateFlags();
	}
	m_InResimulation = false;
}

void PlayerFps::ApplyPhysicsTick(float worldInputX, float worldInputZ, uint32_t buttons, float dt)
{
	// ========================================================================
	// CS:GO / Valorant Style Movement Parameters (match server)
	// ========================================================================
	constexpr float MAX_WALK_SPEED = 5.0f;
	constexpr float MAX_RUN_SPEED  = 8.0f;
	constexpr float GROUND_ACCEL   = 50.0f;
	constexpr float AIR_ACCEL      = 2.0f;
	constexpr float GRAVITY        = 20.0f;
	constexpr float JUMP_VELOCITY  = 8.0f;

	// Input already in world space (converted from camera-relative axes)
	float inputX = worldInputX;
	float inputZ = worldInputZ;
	float inputMag = sqrtf(inputX * inputX + inputZ * inputZ);
	bool tryRunning = (buttons & InputButtons::SPRINT) != 0;
	bool jumpPressed = (buttons & InputButtons::JUMP) != 0;

	float maxSpeed = tryRunning ? MAX_RUN_SPEED : MAX_WALK_SPEED;
	float targetVelX = inputX * maxSpeed;
	float targetVelZ = inputZ * maxSpeed;

	// ====================================================================
	// Ground vs Air Movement
	// ====================================================================
	if (!m_IsJump)  // Grounded
	{
		float accelStep = GROUND_ACCEL * dt;

		float diffX = targetVelX - m_Velocity.x;
		if (fabsf(diffX) <= accelStep)
			m_Velocity.x = targetVelX;
		else
			m_Velocity.x += (diffX > 0 ? accelStep : -accelStep);

		float diffZ = targetVelZ - m_Velocity.z;
		if (fabsf(diffZ) <= accelStep)
			m_Velocity.z = targetVelZ;
		else
			m_Velocity.z += (diffZ > 0 ? accelStep : -accelStep);

		// Jump (consume pending input)
		if (jumpPressed && m_JumpPending)
		{
			if (!m_InResimulation && DebugLog_IsEnabled() && DebugLog_LogJumpEvents())
			{
				DebugLog_Printf("PHYS", "LOCAL_PREDICT_JUMP cTick=%u", m_CurrentClientTick);
			}
			m_Velocity.y = JUMP_VELOCITY;
			m_IsJump = true;
			m_JumpPending = false;
		}
		else if (!m_InResimulation && jumpPressed && DebugLog_IsEnabled() && DebugLog_LogJumpEvents())
		{
			// Jump button is set in this physics tick but we did not jump because pending was already cleared.
			// (Sticky-jump consumed earlier this frame, or never set this tick.)
			DebugLog_Printf("PHYS", "JUMP_NOT_PENDING cTick=%u (grounded but JumpPending=false)",
				m_CurrentClientTick);
		}
	}
	else  // Airborne
	{
		// Pressed jump while client believes it is airborne — local prediction rejects.
		// If this fires repeatedly while server reports IS_GROUNDED (see STATE_MISMATCH rows),
		// it explains "jump fails" — client never applies impulse; server may also reject.
		if (!m_InResimulation && jumpPressed && DebugLog_IsEnabled() && DebugLog_LogJumpEvents())
		{
			DebugLog_Printf("PHYS",
				"LOCAL_REJECT_AIRBORNE cTick=%u m_IsJump=1 (client thinks airborne)",
				m_CurrentClientTick);
		}

		float airStep = AIR_ACCEL * dt;

		if (inputMag > 0.01f)
		{
			m_Velocity.x += inputX * airStep;
			m_Velocity.z += inputZ * airStep;

			float horizSpeed = sqrtf(m_Velocity.x * m_Velocity.x + m_Velocity.z * m_Velocity.z);
			if (horizSpeed > maxSpeed * 1.2f)
			{
				float scale = (maxSpeed * 1.2f) / horizSpeed;
				m_Velocity.x *= scale;
				m_Velocity.z *= scale;
			}
		}

		// Gravity
		m_Velocity.y -= GRAVITY * dt;
	}

	// ====================================================================
	// Apply Velocity to Position
	// ====================================================================
	m_Position.x += m_Velocity.x * dt;
	m_Position.z += m_Velocity.z * dt;
	m_Position.y += m_Velocity.y * dt;

	// ====================================================================
	// Collision Detection (Capsule vs World AABBs)
	// ====================================================================
	if (m_pCollisionWorld)
	{
		auto result = m_pCollisionWorld->ResolveCapsule(
			m_Position, m_Height, m_CapsuleRadius, m_Velocity);
		m_Position = result.position;
		m_Velocity = result.velocity;
		if (result.isGrounded)
			m_IsJump = false;
		else
			m_IsJump = true;
	}
	else
	{
		if (m_Position.y <= 0.0f)
		{
			m_Position.y = 0.0f;
			m_Velocity.y = 0.0f;
			m_IsJump = false;
		}
	}
}


