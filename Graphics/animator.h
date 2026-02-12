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

	// Additive Layer
	void PlayAdditive(const std::string& name, bool loop = false, float weight = 1.0f);
	void PlayAdditive(int index, bool loop = false, float weight = 1.0f);
	void StopAdditive(double fadeOutTime = 0.1);
	void SetAdditiveWeight(float weight) { m_AdditiveTargetWeight = weight; m_AdditiveWeight = weight; }
	bool IsAdditiveActive() const { return m_AdditiveAnimIndex != -1; }

private:
	void UpdateGlobalTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const struct Animation& anim);
	void GetBoneSRT(int boneIndex, const Animation& anim, double time, DirectX::XMVECTOR& outS, DirectX::XMVECTOR& outR, DirectX::XMVECTOR& outT) const;
	void TakeSnapshot(); // 拍下当前所有骨骼的实际姿态（含混合中间状态）
	void ApplyAdditive(int boneIndex, DirectX::XMVECTOR& s, DirectX::XMVECTOR& r, DirectX::XMVECTOR& t);

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

	// Additive Layer
	int    m_AdditiveAnimIndex = -1;
	double m_AdditiveTime = 0.0;
	bool   m_AdditiveLoop = false;
	float  m_AdditiveWeight = 0.0f;
	float  m_AdditiveTargetWeight = 0.0f;
	bool   m_AdditiveFadingOut = false;
	double m_AdditiveFadeTimer = 0.0;
	double m_AdditiveFadeDuration = 0.0;

	// Parallel to m_Model->Bones
	std::vector<DirectX::XMFLOAT4X4> m_GlobalMatrices;
	std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
};
