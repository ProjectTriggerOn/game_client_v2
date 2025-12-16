#pragma once
#include <DirectXMath.h>
#include "collision.h"
#include "model_ani.h"
#include "player_state_mechine.h"

class Player_Fps
{
public:
	Player_Fps();
	~Player_Fps();

	void Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front);
	void Finalize();
	void Update(double elapsed_time);
	void Draw();

	const DirectX::XMFLOAT3& GetPosition() const;
	const DirectX::XMFLOAT3& GetFront() const;
	void SetPosition(const DirectX::XMFLOAT3& position);

	AABB GetAABB() const;
	
	void SetHeight(float height);
	float GetHeight() const;
	
	DirectX::XMFLOAT3 GetEyePosition() const;

private:
	DirectX::XMFLOAT3 m_Position;
	DirectX::XMFLOAT3 m_Velocity;
	DirectX::XMFLOAT3 m_ModelFront;
	DirectX::XMFLOAT3 m_MoveDir;
	DirectX::XMFLOAT3 m_CamRelativePos;
	
	float m_Height;
	bool m_isJump;

	MODEL_ANI* m_Model;

	PlayerStateMachine* m_StateMachine;
};
