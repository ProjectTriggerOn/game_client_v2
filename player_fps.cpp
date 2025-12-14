#include "player_fps.h"
#include "player_cam_fps.h"
#include "key_logger.h"
#include "cube.h"
#include "direct3d.h"
#include "shader_3d_ani.h"

using namespace DirectX;

Player_Fps::Player_Fps()
	: m_Position({ 0,0,0 })
	, m_Velocity({ 0,0,0 })
	, m_ModelFront({ 0,0,1 })
	, m_MoveDir({ 0,0,1 })
	, m_CamRelativePos({ 0.0f, 0.0f, 0.3f })
	, m_Height(2.0f)
	, m_isJump(false)
	, m_Model(nullptr)
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
	
	m_Model = ModelAni_Load("resource/model/arms009.fbx");

	if (!m_Model)
	{
		// Log or handle error if needed
	}
}

void Player_Fps::Finalize()
{
	if (m_Model)
	{
		ModelAni_Release(m_Model);
		m_Model = nullptr;
	}
}

void Player_Fps::Update(double elapsed_time)
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

	bool isMoving = false;
	if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.0f)
	{
		isMoving = true;
		moveDir = XMVector3Normalize(moveDir);
		
		// Apply Movement Velocity
		velocity += moveDir * static_cast<float>(2000.0 / 50.0 * elapsed_time);
		
		XMStoreFloat3(&m_MoveDir, moveDir);
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
	if (m_Model)
	{
		// index 0: Idle, index 1: Walk (Assuming Typical)
		int animIndex = isMoving ? 2 : 0;
		if (m_Model->CurrentAnimationIndex != animIndex && animIndex < (int)m_Model->Animations.size())
		{
			ModelAni_SetAnimation(m_Model, animIndex);
		}
		ModelAni_Update(m_Model, elapsed_time);
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
	world = XMMatrixTranslation(0.0f, -1.13f, 0.0f) * world; // Adjust vertical position if needed

	ModelAni_Draw(m_Model, world, true); // isBlender=false as we constructed the matrix manually
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
