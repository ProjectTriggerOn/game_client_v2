#pragma once
//=============================================================================
// player_fps.h
//
// Local player controlled by user input (Client-Side Prediction).
// 
// Sync Strategy: PREDICTION + CORRECTION
//   - Input applied immediately to m_Position (no lag)
//   - Server corrections applied via render offset (soft) or snap (hard)
//   - m_RenderOffset decays smoothly over time
//=============================================================================

#include <DirectXMath.h>
#include "collision.h"
#include "model_ani.h"
#include "mouse.h"
#include "player_state_mechine.h"
#include "net_common.h"

class Player_Fps
{
public:
	Player_Fps();
	~Player_Fps();

	void Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front);
	void Finalize();
	void Update(double elapsed_time);
	void Draw();

	//-------------------------------------------------------------------------
	// Position Accessors
	//-------------------------------------------------------------------------
	const DirectX::XMFLOAT3& GetPosition() const;      // Logic position (for gameplay)
	DirectX::XMFLOAT3 GetRenderPosition() const;        // Logic + Offset (for rendering)
	const DirectX::XMFLOAT3& GetFront() const;
	void SetPosition(const DirectX::XMFLOAT3& position);
	void SetVelocity(const DirectX::XMFLOAT3& velocity);

	//-------------------------------------------------------------------------
	// Server Reconciliation (Prediction + Correction)
	//-------------------------------------------------------------------------
	void ApplyServerCorrection(const NetPlayerState& serverState);
	
	AABB GetAABB() const;
	
	void SetHeight(float height);
	float GetHeight() const;
	
	DirectX::XMFLOAT3 GetEyePosition() const;

	std::string GetPlayerState() const;
	std::string GetWeaponState() const;
	std::string GetCurrentAniName() const;
	float GetCurrentAniDuration() const;
	float GetCurrentAniProgress() const;

	int GetFireCounter() const { return m_FireCounter; }
	
	//-------------------------------------------------------------------------
	// Debug info
	//-------------------------------------------------------------------------
	const char* GetCorrectionMode() const { return m_CorrectionMode; }
	float GetCorrectionError() const { return m_CorrectionError; }

private:
	// Logic State (authoritative for local player, predicted)
	DirectX::XMFLOAT3 m_Position;         // Logic/Gameplay position
	DirectX::XMFLOAT3 m_Velocity;
	
	// Render Offset (for smooth server correction)
	DirectX::XMFLOAT3 m_RenderOffset;     // Offset that decays over time
	const char* m_CorrectionMode;
	float m_CorrectionError;
	uint32_t m_LastServerTick;
	
	// Other members
	DirectX::XMFLOAT3 m_ModelFront;
	DirectX::XMFLOAT3 m_MoveDir;
	DirectX::XMFLOAT3 m_CamRelativePos;
	float m_Height;
	bool m_isJump;
	double m_WeaponRPM;
	double m_FireTimer;
	int m_FireCounter;
	bool m_TransitionFiring;
	MODEL_ANI* m_Model;
	Animator* m_Animator;
	PlayerStateMachine* m_StateMachine;
};
