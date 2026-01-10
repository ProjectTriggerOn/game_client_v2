#include "player_fps.h"
#include "player_cam_fps.h"
#include "animator.h"
#include "key_logger.h"
#include "cube.h"
#include "direct3d.h"
#include "mouse.h"
#include "shader_3d_ani.h"

using namespace DirectX;

Player_Fps::Player_Fps()
	: m_Position({ 0,0,0 })
	, m_Velocity({ 0,0,0 })
	, m_ModelFront({ 0,0,1 })
	, m_MoveDir({ 0,0,1 })
	, m_CamRelativePos({ 0.0f, 0.0f,0.3f })
	, m_Height(2.0f)
	, m_isJump(false)
	, m_Model(nullptr)
	, m_Animator(nullptr)
	, m_StateMachine(nullptr)
	, m_WeaponRPM(600.0)
	, m_FireCounter(0)
	
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

void Player_Fps::Update(double elapsed_time , const Mouse_State& ms)
{
	XMVECTOR position = XMLoadFloat3(&m_Position);
	XMVECTOR velocity = XMLoadFloat3(&m_Velocity);
	XMVECTOR gravityVelocity = XMVectorZero();

	// Jump
	if (KeyLogger_IsTrigger(KK_SPACE) && !m_isJump)
	{
		velocity += {0.0f, 15.0f, 0.0f};
		m_isJump = true;
	}

	// Gravity
	XMVECTOR gravityDir = { 0.0f, -1.0f };
	velocity += gravityDir * 9.8f * 1.5f * static_cast<float>(elapsed_time);

	gravityVelocity = velocity * static_cast<float>(elapsed_time);
	position += gravityVelocity;

	XMStoreFloat3(&m_Position, position);

	// Collision with Cube (Test environment)
	AABB player = GetAABB();
	AABB cube = Cube_GetAABB({ 3.0f, 0.5f, 2.0f });

	Hit hit = Collision_IsHitAABB(cube, player);
	if (hit.isHit)
	{
		if (hit.normal.y > 0.0f)
		{
			position = XMVectorSetY(position, cube.max.y);
			gravityVelocity = XMVectorZero();
			velocity = XMVectorSetY(velocity, 0.0f);
			m_isJump = false;
		}
	}

	// Floor collision
	if (XMVectorGetY(position) <= 0.0f)
	{
		position = XMVectorSetY(position, 0.0f);
		gravityVelocity = XMVectorZero();
		velocity = XMVectorSetY(velocity, 0.0f);
		m_isJump = false;
	}

	// Movement
	XMVECTOR moveDir = XMVectorZero();
	XMFLOAT3 camFront = PlayerCamFps_GetFront();

	// Update Model Front to match Camera Front (FPS Arms look where camera looks)
	m_ModelFront = camFront;

	// Flatten camera front for movement
	XMVECTOR front = XMVector3Normalize(XMVectorSet(camFront.x, 0.0f, camFront.z, 0.0f));
	XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), front));

	if (KeyLogger_IsPressed(KK_W)) moveDir += front;
	if (KeyLogger_IsPressed(KK_S)) moveDir -= front;
	if (KeyLogger_IsPressed(KK_D)) moveDir += right;
	if (KeyLogger_IsPressed(KK_A)) moveDir -= right;
	bool tryRunning = KeyLogger_IsPressed(KK_LEFTSHIFT);


	//Mouse_State ms;
	//Mouse_GetState(&ms);

	bool isPressingLeft = ms.leftButton;
	bool isPressingRight = ms.rightButton;

	if (isPressingRight && 
		m_StateMachine->GetWeaponState() != WeaponState::ADS && 
		m_StateMachine->GetWeaponState() != WeaponState::ADS_FIRING)
	{
		m_StateMachine->SetWeaponState(WeaponState::ADS_IN);
	}

	if (!isPressingRight && 
		(m_StateMachine->GetWeaponState() == WeaponState::ADS || m_StateMachine->GetWeaponState() == WeaponState::ADS_IN))
	{
		m_StateMachine->SetWeaponState(WeaponState::ADS_OUT);
	}

	if (isPressingLeft)
	{
		if (m_StateMachine->GetWeaponState() == WeaponState::ADS)
		{
			m_Animator->SetSpeed(1.0);
			m_StateMachine->SetWeaponState(WeaponState::ADS_FIRING);

		}
		else if (m_StateMachine->GetWeaponState() == WeaponState::HIP)
		{
			m_Animator->SetSpeed(1.0);
			m_StateMachine->SetWeaponState(WeaponState::HIP_FIRING);
		}
	}
	else
	{
		m_Animator->SetSpeed(1.0);
		if (m_StateMachine->GetWeaponState() == WeaponState::ADS_FIRING)
		{
			m_StateMachine->SetWeaponState(WeaponState::ADS);
		}
		else if (m_StateMachine->GetWeaponState() == WeaponState::HIP_FIRING)
		{
			m_StateMachine->SetWeaponState(WeaponState::HIP);
		}
	}

	if (m_StateMachine->GetWeaponState() == WeaponState::ADS_FIRING)
	{
		if (m_Animator->OnCurrAniStarted())
		{
			m_FireCounter++;
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

	if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0f) {

		moveDir = XMVector3Normalize(moveDir);



		if (tryRunning) {

			m_StateMachine->SetPlayerState(PlayerState::RUNNING);
			float running = static_cast<float>(2000.0 / 50.0 * elapsed_time);
			velocity += moveDir * running;

		}
		else {
			m_StateMachine->SetPlayerState(PlayerState::WALKING);
			float walking = static_cast<float>(1500.0 / 50.0 * elapsed_time);
			velocity += moveDir * walking;
		}

		XMStoreFloat3(&m_MoveDir, moveDir);
	}
	else {
		m_StateMachine->SetPlayerState(PlayerState::IDLE);
	}


	// Friction / Damping
	velocity += -velocity * static_cast<float>(4.0f * elapsed_time);
	position += velocity * static_cast<float>(elapsed_time);

	// Re-check horizontal collision
	if (hit.isHit)
	{
		if (hit.normal.x > 0.0f) position = XMVectorSetX(position, cube.max.x + 1.0f);
		else if (hit.normal.x < 0.0f) position = XMVectorSetX(position, cube.min.x - 1.0f);
		else if (hit.normal.y < 0.0f) position = XMVectorSetY(position, cube.min.y - m_Height);
		else if (hit.normal.z > 0.0f) position = XMVectorSetZ(position, cube.max.z + 1.0f);
		else if (hit.normal.z < 0.0f) position = XMVectorSetZ(position, cube.min.z - 1.0f);
	}

	XMStoreFloat3(&m_Position, position);
	XMStoreFloat3(&m_Velocity, velocity);

	// Update Model Animation
	if (m_Model && m_Animator)
	{
		m_StateMachine->Update(elapsed_time, m_Animator);
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
	switch (m_StateMachine->GetPlayerState())
	{
	case PlayerState::IDLE:
		return "IDLE";
	case PlayerState::WALKING:
		return "WALKING";
	case PlayerState::RUNNING:
		return "RUNNING";
	default:
		return "UNKNOWN";
	}
}

std::string Player_Fps::GetWeaponState() const
{
	switch (m_StateMachine->GetWeaponState())
	{
	case WeaponState::HIP:
		return "HIP";
	case WeaponState::ADS_IN:
		return "ADS_IN";
	case WeaponState::ADS:
		return "ADS";
	case WeaponState::ADS_OUT:
		return "ADS_OUT";
	case WeaponState::HIP_FIRING:
		return "HIP_FIRING";
	case WeaponState::ADS_FIRING:
		return "ADS_FIRING";
	case WeaponState::RELOADING:
		return "RELOADING";
	case WeaponState::RELOADING_OUT_OF_AMMO:
		return "RELOADING_OUT_OF_AMMO";
	case WeaponState::INSPECTING:
		return "INSPECTING";
	case WeaponState::TAKING_OUT:
		return "TAKING_OUT";
	default:
		return "UNKNOWN";
	}
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
