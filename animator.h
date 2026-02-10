#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>
#include <functional>

struct MODEL_ANI;

class Animator
{
public:
	Animator();
	~Animator();

	void Init(MODEL_ANI* model);
	void Update(double elapsedTime);
	void Play(const std::string& name, bool loop = true);
	void Play(int index, bool loop = true);
	void PlayCrossFade(const std::string& name, bool loop = true, double blendTime = 0.2);
	void PlayCrossFade(int index, bool loop = true, double blendTime = 0.2);
	bool IsCurrAniFinished() const;
	bool OnCurrAniStarted() const;
	float GetCurrAniProgress() const;
	double GetSpeed() const { return m_Speed; }
	void SetSpeed(double speed) { m_Speed = speed; }
	bool IsBlending() const { return m_IsBlending; }

	// Returns the matrices to be sent to the shader
	const std::vector<DirectX::XMFLOAT4X4>& GetFinalBoneMatrices() const;

	int GetCurrentAnimationIndex() const { return m_CurrentAnimationIndex; }

	using AnimationCallBack = std::function<void(const std::string&)>;
	void SetOnAnimationCallBack(AnimationCallBack callback) { m_OnAnimationStart = callback; }

	void SetSameAniOverlapAllow(bool allow) { m_SameAniOverlapAllow = allow; }

private:
	void UpdateGlobalTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const struct Animation& anim);
	void GetBoneSRT(int boneIndex, const Animation& anim, double time, DirectX::XMVECTOR& outS, DirectX::XMVECTOR& outR, DirectX::XMVECTOR& outT) const;
	void TakeSnapshot(); // 拍下当前所有骨骼的实际姿态（含混合中间状态）

private:
	MODEL_ANI* m_Model;
	
	int m_CurrentAnimationIndex;
	double m_CurrentTime;
	bool m_Loop;
	double m_Speed = 1.0;

	// Cross Fade — Snapshot-based blending
	struct BoneSRT {
		DirectX::XMFLOAT4 scale;       // stored as XMFLOAT4 for XMVector load/store
		DirectX::XMFLOAT4 rotation;    // quaternion
		DirectX::XMFLOAT4 translation;
	};
	std::vector<BoneSRT> m_SnapshotPose; // per-bone snapshot at transition start

	bool m_IsBlending;
	double m_TransitionTime;
	double m_TransitionDuration;

	bool m_SameAniOverlapAllow = false;

	AnimationCallBack m_OnAnimationStart;

	// Parallel to m_Model->Bones
	std::vector<DirectX::XMFLOAT4X4> m_GlobalMatrices;
	std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
};
