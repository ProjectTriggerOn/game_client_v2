#pragma once

#include <DirectXMath.h>
#include <vector>
#include <string>

struct MODEL_ANI;

class Animator
{
public:
	Animator();
	~Animator();

	void Init(MODEL_ANI* model);
	void Update(double elapsedTime);
	void Play(const std::string& name, bool loop = true, double blendTime = 0.2);
	void Play(int index, bool loop = true, double blendTime = 0.2);

	// Returns the matrices to be sent to the shader
	const std::vector<DirectX::XMFLOAT4X4>& GetFinalBoneMatrices() const;

	int GetCurrentAnimationIndex() const { return m_CurrentAnimationIndex; }

private:
	void UpdateGlobalTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const struct Animation& anim);
	void GetBoneSRT(int boneIndex, const struct Animation& anim, double time, DirectX::XMVECTOR& outS, DirectX::XMVECTOR& outR, DirectX::XMVECTOR& outT) const;

private:
	MODEL_ANI* m_Model;
	
	int m_CurrentAnimationIndex;
	double m_CurrentTime;
	bool m_Loop;

	// Cross Fade (Blending)
	int m_PrevAnimationIndex;
	double m_PrevTime;
	bool m_PrevLoop;
	
	bool m_IsBlending;
	double m_BlendFactor; // 0.0 to 1.0 (0.0 = Prev, 1.0 = Curr) -- actually tracking time is better
	double m_TransitionTime;
	double m_TransitionDuration;

	// Parallel to m_Model->Bones
	std::vector<DirectX::XMFLOAT4X4> m_GlobalMatrices;
	std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
};
