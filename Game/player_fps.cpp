#include "player_fps.h"
#include "player_cam_fps.h"
#include "animator.h"
#include "key_logger.h"
#include "cube.h"
#include "direct3d.h"
#include "game.h"
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
	, m_ModelFront({ 0,0,1 })
	, m_MoveDir({ 0,0,1 })
	, m_CamRelativePos({ 0.0f, 0.0f,0.3f })
	, m_Height(2.0f)
	, m_isJump(false)
	, m_Model(nullptr)
	, m_Animator(nullptr)
	, m_StateMachine(nullptr)
	, m_WeaponRPM(600.0)
	, m_FireTimer(0.0)
	, m_FireCounter(0)
	, m_TransitionFiring(false)
	
{
}

Player_Fps::~Player_Fps()
{
	Finalize();
}

void Player_Fps::Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front)
{
	m_Position = position;
	m_Velocity = { 0.0f, 0.0f, 0.0f };
	m_RenderOffset = { 0.0f, 0.0f, 0.0f };
	m_CorrectionMode = "NONE";
	m_CorrectionError = 0.0f;
	m_LastServerTick = 0;

	XMStoreFloat3(&m_MoveDir, XMVector3Normalize(XMLoadFloat3(&front)));
	XMStoreFloat3(&m_ModelFront, XMVector3Normalize(XMLoadFloat3(&front)));

	m_StateMachine = new PlayerStateMachine();
	
	m_Model = ModelAni_Load("resource/model/arms009.fbx");

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

void Player_Fps::Update(double elapsed_time)
{
	const float dt = static_cast<float>(elapsed_time);
	
	// ========================================================================
	// Render Offset Decay (for smooth server correction)
	// Decays visual offset toward zero over ~100ms
	// ========================================================================
	constexpr float OFFSET_DECAY_RATE = 10.0f;  // Higher = faster decay
	float decayFactor = 1.0f - OFFSET_DECAY_RATE * dt;
	if (decayFactor < 0.0f) decayFactor = 0.0f;
	m_RenderOffset.x *= decayFactor;
	m_RenderOffset.y *= decayFactor;
	m_RenderOffset.z *= decayFactor;
	
	// ========================================================================
	// CS:GO / Valorant Style Movement Parameters (match mock_server.cpp)
	// ========================================================================
	constexpr float MAX_WALK_SPEED = 5.0f;
	constexpr float MAX_RUN_SPEED  = 8.0f;
	constexpr float GROUND_ACCEL   = 50.0f;
	constexpr float AIR_ACCEL      = 2.0f;
	constexpr float GRAVITY        = 20.0f;
	constexpr float JUMP_VELOCITY  = 8.0f;
	
	// ========================================================================
	// Calculate Target Velocity from Input
	// ========================================================================
	XMFLOAT3 camFront = PlayerCamFps_GetFront();
	m_ModelFront = camFront;  // FPS Arms look where camera looks
	
	// Flatten camera front for movement
	float frontX = camFront.x;
	float frontZ = camFront.z;
	float frontMag = sqrtf(frontX * frontX + frontZ * frontZ);
	if (frontMag > 0.001f) { frontX /= frontMag; frontZ /= frontMag; }
	
	float rightX = frontZ;
	float rightZ = -frontX;
	
	// Input direction
	float inputX = 0.0f, inputZ = 0.0f;
	if (KeyLogger_IsPressed(KK_W)) { inputX += frontX; inputZ += frontZ; }
	if (KeyLogger_IsPressed(KK_S)) { inputX -= frontX; inputZ -= frontZ; }
	if (KeyLogger_IsPressed(KK_D)) { inputX += rightX; inputZ += rightZ; }
	if (KeyLogger_IsPressed(KK_A)) { inputX -= rightX; inputZ -= rightZ; }
	
	// Normalize diagonal
	float inputMag = sqrtf(inputX * inputX + inputZ * inputZ);
	if (inputMag > 1.0f) { inputX /= inputMag; inputZ /= inputMag; inputMag = 1.0f; }
	
	bool tryRunning = KeyLogger_IsPressed(KK_LEFTSHIFT);
	float maxSpeed = tryRunning ? MAX_RUN_SPEED : MAX_WALK_SPEED;
	
	float targetVelX = inputX * maxSpeed;
	float targetVelZ = inputZ * maxSpeed;
	
	// ========================================================================
	// Ground vs Air Movement
	// ========================================================================
	if (!m_isJump)  // Grounded
	{
		// GROUND: Snappy, high acceleration, instant stop
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
		
		// Jump
		if (KeyLogger_IsTrigger(KK_SPACE))
		{
			m_Velocity.y = JUMP_VELOCITY;
			m_isJump = true;
		}
	}
	else  // Airborne
	{
		// AIR: Limited control, momentum preserved
		float airStep = AIR_ACCEL * dt;
		
		if (inputMag > 0.01f)
		{
			m_Velocity.x += inputX * airStep;
			m_Velocity.z += inputZ * airStep;
			
			// Cap speed
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
	
	// ========================================================================
	// Apply Velocity to Position
	// ========================================================================
	m_Position.x += m_Velocity.x * dt;
	m_Position.z += m_Velocity.z * dt;
	m_Position.y += m_Velocity.y * dt;
	
	// ========================================================================
	// Floor Collision
	// ========================================================================
	if (m_Position.y <= 0.0f)
	{
		m_Position.y = 0.0f;
		m_Velocity.y = 0.0f;
		m_isJump = false;
	}
	
	// ========================================================================
	// Update Player State for Animations
	// ========================================================================
	if (inputMag > 0.01f)
	{
		m_StateMachine->SetPlayerState(tryRunning ? PlayerState::RUNNING : PlayerState::WALKING);
		XMStoreFloat3(&m_MoveDir, XMVectorSet(inputX, 0, inputZ, 0));
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
		m_StateMachine->GetWeaponState() != WeaponState::ADS_FIRING)
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
				if (Game_GetState() == PLAY) {
					m_FireCounter++;
				}
			} else {
				// Full-auto at RPM interval
				m_FireTimer += dt;
				const double fireInterval = 60.0 / m_WeaponRPM;
				if (m_FireTimer >= fireInterval) {
					m_FireTimer -= fireInterval;
					if (Game_GetState() == PLAY) {
						m_FireCounter++;
					}
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
		// First shot on press
		m_StateMachine->SetWeaponState(WeaponState::ADS_FIRING);
		m_FireTimer = 0.0;
		if (Game_GetState() == PLAY) {
			m_FireCounter++;
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
			m_FireTimer += dt;
			const double fireInterval = 60.0 / m_WeaponRPM;
			if (m_FireTimer >= fireInterval) {
				m_FireTimer -= fireInterval;
				m_Animator->SetSameAniOverlapAllow(true);
				if (Game_GetState() == PLAY) {
					m_FireCounter++;
				}
			}
		}
	}

	// ---- HIP FIRE: click to start, hold for full-auto ----
	if (isPressingLeft && m_StateMachine->GetWeaponState() == WeaponState::HIP) {
		// First shot on press
		m_StateMachine->SetWeaponState(WeaponState::HIP_FIRING);
		m_FireTimer = 0.0;
		if (Game_GetState() == PLAY) {
			m_FireCounter++;
		}
	}

	if (m_StateMachine->GetWeaponState() == WeaponState::HIP_FIRING) {
		if (isPressingLeft) {
			// Full-auto: accumulate timer and fire at RPM interval
			m_FireTimer += dt;
			const double fireInterval = 60.0 / m_WeaponRPM;
			if (m_FireTimer >= fireInterval) {
				m_FireTimer -= fireInterval;
				m_Animator->SetSameAniOverlapAllow(true);
				if (Game_GetState() == PLAY) {
					m_FireCounter++;
				}
			}
		}
	}

	if (KeyLogger_IsTrigger(KK_R))
	{
		m_StateMachine->SetWeaponState(WeaponState::RELOADING);
	}

	if (KeyLogger_IsTrigger(KK_E))
	{
		m_StateMachine->SetWeaponState(WeaponState::INSPECTING);
	}

	// Update Model Animation
	if (m_Model && m_Animator)
	{
		m_StateMachine->Update(elapsed_time, m_Animator);

		// Override additive layer: fire animation during ADS transition
		if (m_TransitionFiring)
		{
			// PlayAdditive with fire animation (index 3), non-loop, full weight
			m_Animator->PlayAdditive(3, false, 1.0f);
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
// GetRenderPosition - Logic position + visual offset (for smooth correction)
//-----------------------------------------------------------------------------
DirectX::XMFLOAT3 Player_Fps::GetRenderPosition() const
{
	return {
		m_Position.x + m_RenderOffset.x,
		m_Position.y + m_RenderOffset.y,
		m_Position.z + m_RenderOffset.z
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
	m_LastServerTick = serverState.tickId;
	
	// Correction thresholds
	constexpr float SOFT_CORRECTION_THRESHOLD = 0.5f;  // < 0.5m -> soft correction
	constexpr float HARD_SNAP_THRESHOLD = 3.0f;         // > 3.0m -> hard snap
	
	// Calculate error between predicted and server position
	float dx = m_Position.x - serverState.position.x;
	float dy = m_Position.y - serverState.position.y;
	float dz = m_Position.z - serverState.position.z;
	float error = sqrtf(dx * dx + dy * dy + dz * dz);
	
	m_CorrectionError = error;
	
	if (error > HARD_SNAP_THRESHOLD)
	{
		// ===== HARD SNAP =====
		// Error too large, teleport immediately
		m_CorrectionMode = "SNAP";
		m_Position = serverState.position;
		m_Velocity = serverState.velocity;
		m_RenderOffset = { 0.0f, 0.0f, 0.0f };
	}
	else if (error > SOFT_CORRECTION_THRESHOLD)
	{
		// ===== SOFT CORRECTION =====
		// Medium error: snap logic position, add visual offset
		m_CorrectionMode = "SOFT";
		
		// Calculate visual offset = where we WERE - where we SHOULD BE
		m_RenderOffset.x += m_Position.x - serverState.position.x;
		m_RenderOffset.y += m_Position.y - serverState.position.y;
		m_RenderOffset.z += m_Position.z - serverState.position.z;
		
		// Snap logic position to server
		m_Position = serverState.position;
		m_Velocity = serverState.velocity;
	}
	else
	{
		// ===== NO CORRECTION =====
		// Error small enough, prediction is accurate
		m_CorrectionMode = "OK";
		// Keep m_Position as-is (client prediction is good)
	}
}

AABB Player_Fps::GetAABB() const
{
	return {
		{m_Position.x - 1.0f, m_Position.y, m_Position.z - 1.0f },
		{m_Position.x + 1.0f, m_Position.y + m_Height, m_Position.z + 1.0f}
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
	// Calculate eye position based on height (e.g., 90% of height)
	DirectX::XMFLOAT3 eyePos = m_Position;
	eyePos.y += m_Height * 0.9f; 
	// Or use the fixed 1.7f if preferred, but m_Height * 0.9f scales with height.
	// Given default height 2.0, 0.9 = 1.8. 
	// If 1.7 is desired, ratio is 0.85. 
	// I'll stick to 0.85f to match the previous ~1.7f approximation (2.0 * 0.85 = 1.7).
	eyePos.y = m_Position.y + (m_Height * 0.85f);
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


