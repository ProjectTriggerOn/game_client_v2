#include "player_fps.h"
#include "player_cam_fps.h"
#include "animator.h"
#include "key_logger.h"
#include "cube.h"
#include "direct3d.h"
#include "fade.h"
#include "game.h"
#include "input_producer.h"
#include "mouse.h"
#include "ms_logger.h"
#include "shader_3d_ani.h"

using namespace DirectX;

Player_Fps::Player_Fps()
	: m_Position({ 0,0,0 })
	, m_Velocity({ 0,0,0 })
	, m_RenderOffset({ 0,0,0 })
	, m_CorrectionMode("NONE")
	, m_CorrectionError(0.0f)
	, m_LastServerTick(0)
	, m_InputHistoryHead(0)
	, m_InputHistoryCount(0)
	, m_CurrentClientTick(0)
	, m_ModelFront({ 0,0,1 })
	, m_MoveDir({ 0,0,1 })
	, m_CamRelativePos({ 0.0f, 0.0f,0.3f })
	, m_Height(1.6f)
	, m_CapsuleRadius(0.3f)
	, m_isJump(false)
	, m_JumpPending(false)
	, m_pCollisionWorld(nullptr)
	, m_PhysicsAccumulator(0.0)
	, m_PrevPhysicsPosition({ 0,0,0 })
	, m_PhysicsAlpha(0.0f)
	, m_Model(nullptr)
	, m_Animator(nullptr)
	, m_StateMachine(nullptr)
	, m_Ammo(MAG_SIZE)
	, m_AmmoReserve(MAX_RESERVE)
	, m_InfiniteReserve(true)
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

Player_Fps::~Player_Fps()
{
	Finalize();
}

void Player_Fps::Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front,
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
	m_Ammo        = MAG_SIZE;
	m_AmmoReserve = MAX_RESERVE;

	XMStoreFloat3(&m_MoveDir, XMVector3Normalize(XMLoadFloat3(&front)));
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

void Player_Fps::Finalize()
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

void Player_Fps::SetTeam(uint8_t teamId)
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

void Player_Fps::Update(double elapsed_time)
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
		XMStoreFloat3(&m_MoveDir, XMVectorSet(worldInputX, 0, worldInputZ, 0));
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
		} else if ((m_InfiniteReserve || m_AmmoReserve > 0)) {
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
				else if ((m_InfiniteReserve || m_AmmoReserve > 0)) {
					m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
				}
			}
		}
	}

	// ---- HIP FIRE: click to start, hold for full-auto ----
	if (isPressingLeft && m_StateMachine->GetWeaponState() == WeaponState::HIP) {
		if (m_Ammo > 0) {
			// First shot on press
			m_StateMachine->SetWeaponState(WeaponState::HIP_FIRING);
			m_FireTimer = 0.0;
			m_Ammo--; m_FireCounter++;
		} else if ((m_InfiniteReserve || m_AmmoReserve > 0)) {
			m_StateMachine->SetWeaponState(WeaponState::RELOADING_OUT_OF_AMMO);
		}
	}

	if (m_StateMachine->GetWeaponState() == WeaponState::HIP_FIRING) {
		if (isPressingLeft) {
			// Full-auto: accumulate timer and fire at RPM interval
			m_FireTimer += frameDt;
			const double fireInterval = 60.0 / m_WeaponRPM;
			if (m_FireTimer >= fireInterval) {
				m_FireTimer -= fireInterval;
				m_Animator->SetSameAniOverlapAllow(true);
				if (m_Ammo > 0) { m_Ammo--; m_FireCounter++; }
				else if ((m_InfiniteReserve || m_AmmoReserve > 0)) {
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
		if (!alreadyReloading && m_Ammo < MAG_SIZE && (m_InfiniteReserve || m_AmmoReserve > 0))
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
			int needed = MAG_SIZE - m_Ammo;
			if (m_InfiniteReserve) {
				m_Ammo = MAG_SIZE;
			} else {
				int refill    = (m_AmmoReserve >= needed) ? needed : m_AmmoReserve;
				m_Ammo       += refill;
				m_AmmoReserve -= refill;
			}
		}

		// Override additive layer: fire animation during ADS transition
		if (m_TransitionFiring)
		{
			// PlayAdditive with fire animation (index 4), non-loop, full weight
			m_Animator->PlayAdditive(4, false, 1.0f);
		}
	}

}

void Player_Fps::Draw()
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

const DirectX::XMFLOAT3& Player_Fps::GetPosition() const
{
	return m_Position;
}

const DirectX::XMFLOAT3& Player_Fps::GetFront() const
{
	return m_ModelFront;
}

void Player_Fps::SetPosition(const DirectX::XMFLOAT3& position)
{
	m_Position = position;
}

void Player_Fps::SetVelocity(const DirectX::XMFLOAT3& velocity)
{
	m_Velocity = velocity;
}

//-----------------------------------------------------------------------------
// GetRenderPosition - Sub-tick interpolated position + server correction offset
//   Lerp between previous and current physics position using accumulator remainder
//   Then add server correction render offset on top
//-----------------------------------------------------------------------------
DirectX::XMFLOAT3 Player_Fps::GetRenderPosition() const
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
void Player_Fps::ApplyServerCorrection(const NetPlayerState& serverState)
{
	// Skip if same tick already processed
	if (serverState.tickId <= m_LastServerTick && m_LastServerTick != 0)
		return;

	// Initialize client tick from first server snapshot
	if (m_CurrentClientTick == 0)
	{
		m_CurrentClientTick = serverState.tickId;
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
		m_isJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

		// Clear input history on hard snap
		ClearInputHistory();
	}
	else if (error > RESIM_THRESHOLD)
	{
		// ===== RE-SIMULATION =====
		// Find history entry for server tick
		InputHistoryEntry* serverEntry = FindHistoryEntry(serverState.tickId);

		if (!serverEntry)
		{
			// Tick not in history (too old or missing)
			// Fall back to old soft correction behavior
			m_CorrectionMode = "SOFT";
			m_RenderOffset.x += m_Position.x - serverState.position.x;
			m_RenderOffset.y += m_Position.y - serverState.position.y;
			m_RenderOffset.z += m_Position.z - serverState.position.z;
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
			m_isJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

			// Re-simulate all ticks from server tick to current tick
			ResimulateFromTick(serverState.tickId);

			// Verify re-simulation quality: check server tick position after re-sim
			InputHistoryEntry* verifyEntry = FindHistoryEntry(serverState.tickId);
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
		m_isJump = !(serverState.stateFlags & NetStateFlags::IS_GROUNDED);

		// Face toward world center (0, 0, 0)
		float dx = 0.0f - serverState.position.x;
		float dz = 0.0f - serverState.position.z;
		float yaw = atan2f(dx, dz);
		PlayerCamFps_SetYaw(yaw);
		PlayerCamFps_SetPitch(0.0f);

		// Reset ammo and weapon state on respawn
		m_Ammo        = MAG_SIZE;
		m_AmmoReserve = MAX_RESERVE;
		m_StateMachine->SetWeaponState(WeaponState::HIP);

		// Clear input history and sync tick on respawn
		ClearInputHistory();
		m_CurrentClientTick = serverState.tickId;
	}
	m_WasDead = isDead;
}

AABB Player_Fps::GetAABB() const
{
	return {
		{m_Position.x - 1.0f, m_Position.y, m_Position.z - 1.0f },
		{m_Position.x + 1.0f, m_Position.y + m_Height, m_Position.z + 1.0f}
	};
}

Capsule Player_Fps::GetCapsule() const
{
	return {
		{ m_Position.x, m_Position.y + m_CapsuleRadius, m_Position.z },
		{ m_Position.x, m_Position.y + m_Height - m_CapsuleRadius, m_Position.z },
		m_CapsuleRadius
	};
}

void Player_Fps::SetHeight(float height)
{
	m_Height = height;
}

float Player_Fps::GetHeight() const
{
	return m_Height;
}

DirectX::XMFLOAT3 Player_Fps::GetEyePosition() const
{
	// Use interpolated position for smooth camera
	float a = m_PhysicsAlpha;
	DirectX::XMFLOAT3 eyePos;
	eyePos.x = m_PrevPhysicsPosition.x + (m_Position.x - m_PrevPhysicsPosition.x) * a;
	eyePos.y = m_PrevPhysicsPosition.y + (m_Position.y - m_PrevPhysicsPosition.y) * a + 1.5f;
	eyePos.z = m_PrevPhysicsPosition.z + (m_Position.z - m_PrevPhysicsPosition.z) * a;
	return eyePos;
}

std::string Player_Fps::GetPlayerState() const
{
	return PlayerStateToString(m_StateMachine->GetPlayerState());
}

std::string Player_Fps::GetWeaponState() const
{
	return WeaponStateToString(m_StateMachine->GetWeaponState());
}

std::string Player_Fps::GetCurrentAniName() const
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

float Player_Fps::GetCurrentAniDuration() const
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

float Player_Fps::GetCurrentAniProgress() const
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

void Player_Fps::RecordInputHistory(const InputCmd& cmd, float worldInputX, float worldInputZ)
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

InputHistoryEntry* Player_Fps::FindHistoryEntry(uint32_t tickId)
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

void Player_Fps::ClearInputHistory()
{
	m_InputHistoryHead = 0;
	m_InputHistoryCount = 0;
}

uint32_t Player_Fps::GetStateFlags() const
{
	uint32_t flags = 0;
	if (m_isJump)
		flags |= NetStateFlags::IS_JUMPING;
	if (!m_isJump)
		flags |= NetStateFlags::IS_GROUNDED;
	// Add other state flags as needed (IS_FIRING, IS_ADS, etc.)
	return flags;
}

void Player_Fps::ResimulateFromTick(uint32_t serverTick)
{
	// Replay all ticks from serverTick+1 to m_CurrentClientTick
	// This corrects client prediction based on server's authoritative state
	const float dt = static_cast<float>(TICK_DURATION);

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
}

void Player_Fps::ApplyPhysicsTick(float worldInputX, float worldInputZ, uint32_t buttons, float dt)
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
	if (!m_isJump)  // Grounded
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
			m_Velocity.y = JUMP_VELOCITY;
			m_isJump = true;
			m_JumpPending = false;
		}
	}
	else  // Airborne
	{
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
			m_isJump = false;
		else
			m_isJump = true;
	}
	else
	{
		if (m_Position.y <= 0.0f)
		{
			m_Position.y = 0.0f;
			m_Velocity.y = 0.0f;
			m_isJump = false;
		}
	}
}


