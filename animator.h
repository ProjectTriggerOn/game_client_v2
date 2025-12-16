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
	void Play(const std::string& name, bool loop = true);
	void Play(int index, bool loop = true);

	// Returns the matrices to be sent to the shader
	const std::vector<DirectX::XMFLOAT4X4>& GetFinalBoneMatrices() const;

	int GetCurrentAnimationIndex() const { return m_CurrentAnimationIndex; }

private:
	void UpdateBoneTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const struct Animation& anim);

private:
	MODEL_ANI* m_Model;
	int m_CurrentAnimationIndex;
	double m_CurrentTime;
	bool m_Loop;

	// Parallel to m_Model->Bones
	std::vector<DirectX::XMFLOAT4X4> m_GlobalMatrices;
	std::vector<DirectX::XMFLOAT4X4> m_FinalBoneMatrices;
};
