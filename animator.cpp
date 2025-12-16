#include "animator.h"
#include "model_ani.h"
#include <cmath>
#include <algorithm>

using namespace DirectX;

namespace
{
	// Helper to find keyframe index
	template <typename T>
	int FindKeyIndex(double animationTime, const std::vector<T>& keys)
	{
		for (size_t i = 0; i < keys.size() - 1; i++)
		{
			if (animationTime < keys[i + 1].time)
				return (int)i;
		}
		return 0;
	}
}

Animator::Animator()
	: m_Model(nullptr), m_CurrentAnimationIndex(-1), m_CurrentTime(0.0), m_Loop(true)
{
}

Animator::~Animator()
{
}

void Animator::Init(MODEL_ANI* model)
{
	m_Model = model;
	if (m_Model)
	{
		size_t boneCount = m_Model->Bones.size();
		m_GlobalMatrices.resize(boneCount);
		m_FinalBoneMatrices.resize(boneCount);

		// Initialize with identity
		for (size_t i = 0; i < boneCount; ++i)
		{
			XMStoreFloat4x4(&m_GlobalMatrices[i], XMMatrixIdentity());
			XMStoreFloat4x4(&m_FinalBoneMatrices[i], XMMatrixIdentity());
		}
		
		if (!m_Model->Animations.empty())
		{
			Play(0, true);
		}
	}
}

void Animator::Play(const std::string& name, bool loop)
{
	if (!m_Model) return;

	for (int i = 0; i < (int)m_Model->Animations.size(); ++i)
	{
		if (m_Model->Animations[i].name == name)
		{
			Play(i, loop);
			return;
		}
	}
}

void Animator::Play(int index, bool loop)
{
	if (!m_Model) return;
	if (index >= 0 && index < (int)m_Model->Animations.size())
	{
		m_CurrentAnimationIndex = index;
		m_CurrentTime = 0.0;
		m_Loop = loop;
	}
}

void Animator::Update(double elapsedTime)
{
	if (!m_Model || m_CurrentAnimationIndex == -1) return;
	if (m_Model->Animations.empty()) return;

	const Animation& anim = m_Model->Animations[m_CurrentAnimationIndex];
	
	m_CurrentTime += elapsedTime * anim.ticksPerSecond;
	
	if (m_Loop)
	{
		m_CurrentTime = fmod(m_CurrentTime, anim.duration);
	}
	else
	{
		if (m_CurrentTime >= anim.duration)
		{
			m_CurrentTime = anim.duration;
		}
	}

	// Calculate transformations
	// Identify root bones (parentIndex == -1) and traverse
	for (int i = 0; i < (int)m_Model->Bones.size(); ++i)
	{
		if (m_Model->Bones[i].parentIndex == -1)
		{
			UpdateBoneTransforms(i, XMMatrixIdentity(), anim);
		}
	}
}

void Animator::UpdateBoneTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const Animation& anim)
{
	const Bone& boneDef = m_Model->Bones[boneIndex];
	
	// Start with local transformation from bind pose? 
	// Or identity. The AnimationChannel will override it if present.
	// If the bone is NOT animated, it should probably stick to its default local transform (Bind Pose).
	// Currently Model_Ani stores `localMatrix` in Bone which was initialized to node transform.
	// Since we are decoupling, we should read that initialization value. 
	// But `Bone` struct in `model_ani.h` has `localMatrix` which currently changes every frame in the old code.
	// I need to be careful. In `ModelAni_Load`, `bone.localMatrix` is initialized to the NODE transformation. 
	// So we should assume that is the default.
	
	XMMATRIX nodeTransform = XMLoadFloat4x4(&boneDef.localMatrix);
	
	// Find animation channel for this bone
	const AnimationChannel* channel = nullptr;
	for (const auto& ch : anim.channels)
	{
		if (ch.boneIndex == boneIndex)
		{
			channel = &ch;
			break;
		}
	}

	if (channel)
	{
		// Interpolate Position
		XMVECTOR pos = XMVectorSet(0, 0, 0, 1);
		if (!channel->positionKeys.empty()) {
			int idx = FindKeyIndex(m_CurrentTime, channel->positionKeys);
			int nextIdx = (idx + 1) % channel->positionKeys.size();
			double t1 = channel->positionKeys[idx].time;
			double t2 = channel->positionKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((m_CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;
			
			XMVECTOR p1 = XMLoadFloat3(&channel->positionKeys[idx].value);
			XMVECTOR p2 = XMLoadFloat3(&channel->positionKeys[nextIdx].value);
			pos = XMVectorLerp(p1, p2, factor);
		}
		
		// Interpolate Rotation
		XMVECTOR rot = XMVectorSet(0, 0, 0, 1);
		if (!channel->rotationKeys.empty()) {
			int idx = FindKeyIndex(m_CurrentTime, channel->rotationKeys);
			int nextIdx = (idx + 1) % channel->rotationKeys.size();
			double t1 = channel->rotationKeys[idx].time;
			double t2 = channel->rotationKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((m_CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;

			XMVECTOR r1 = XMLoadFloat4(&channel->rotationKeys[idx].value);
			XMVECTOR r2 = XMLoadFloat4(&channel->rotationKeys[nextIdx].value);
			rot = XMQuaternionSlerp(r1, r2, factor);
		}
		
		// Interpolate Scale
		XMVECTOR scale = XMVectorSet(1, 1, 1, 1);
		if (!channel->scalingKeys.empty()) {
			int idx = FindKeyIndex(m_CurrentTime, channel->scalingKeys);
			int nextIdx = (idx + 1) % channel->scalingKeys.size();
			double t1 = channel->scalingKeys[idx].time;
			double t2 = channel->scalingKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((m_CurrentTime - t1) / dt);
			if (dt == 0) factor = 0;

			XMVECTOR s1 = XMLoadFloat3(&channel->scalingKeys[idx].value);
			XMVECTOR s2 = XMLoadFloat3(&channel->scalingKeys[nextIdx].value);
			scale = XMVectorLerp(s1, s2, factor);
		}

		nodeTransform = XMMatrixScalingFromVector(scale) * XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);
	}

	XMMATRIX globalTransform = nodeTransform * parentTransform;
	XMStoreFloat4x4(&m_GlobalMatrices[boneIndex], globalTransform);

	// Calculate Final Matrix
	XMMATRIX offset = XMLoadFloat4x4(&boneDef.offsetMatrix);
	XMMATRIX finalM = offset * globalTransform;
	XMStoreFloat4x4(&m_FinalBoneMatrices[boneIndex], finalM);

	for (int childIndex : boneDef.children)
	{
		UpdateBoneTransforms(childIndex, globalTransform, anim);
	}
}

const std::vector<DirectX::XMFLOAT4X4>& Animator::GetFinalBoneMatrices() const
{
	return m_FinalBoneMatrices;
}
