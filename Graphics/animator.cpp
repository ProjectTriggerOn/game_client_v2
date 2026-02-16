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
	: m_Model(nullptr)
	, m_CurrentAnimationIndex(-1), m_CurrentTime(0.0), m_Loop(true)
	, m_IsBlending(false), m_TransitionTime(0.0), m_TransitionDuration(0.0)
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
		m_SnapshotPose.resize(boneCount);

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

void Animator::PlayCrossFade(const std::string& name, bool loop, double blendTime)
{
	if (!m_Model) return;

	for (int i = 0; i < (int)m_Model->Animations.size(); ++i)
	{
		if (m_Model->Animations[i].name == name)
		{
			PlayCrossFade(i, loop, blendTime);
			return;
		}
	}
}

void Animator::PlayCrossFade(int index, bool loop, double blendTime)
{
	if (!m_Model) return;

	// 1. 如果目标就是当前正在播的主动画，只更新循环状态，不重置
	if (index == m_CurrentAnimationIndex)
	{
		// 如果允许同个动画叠加 (Self-Overlap)
		if (m_SameAniOverlapAllow)
		{
			if (blendTime > 0.0)
			{
				TakeSnapshot();

				m_CurrentAnimationIndex = index;
				m_CurrentTime = 0.0;
				m_Loop = loop;

				m_IsBlending = true;
				m_TransitionDuration = blendTime;
				m_TransitionTime = 0.0;
			}
			else
			{
				m_CurrentTime = 0.0;
				m_Loop = loop;
				m_IsBlending = false;
			}
			
			// Auto-consume the flag so we don't keep restarting every frame
			m_SameAniOverlapAllow = false;
			return;
		}

		m_Loop = loop;
		return;
	}

	if (index >= 0 && index < (int)m_Model->Animations.size())
	{
		if (blendTime > 0.0 && m_CurrentAnimationIndex != -1)
		{
			// 拍下当前视觉姿态的快照（无论是否正在混合中）
			TakeSnapshot();

			m_CurrentAnimationIndex = index;
			m_CurrentTime = 0.0;
			m_Loop = loop;

			m_IsBlending = true;
			m_TransitionDuration = blendTime;
			m_TransitionTime = 0.0;
		}
		else
		{
			// 无混合直接切换
			m_CurrentAnimationIndex = index;
			m_CurrentTime = 0.0;
			m_Loop = loop;
			m_IsBlending = false;
		}
	}
}

bool Animator::IsCurrAniFinished() const
{
	if (!m_Model || m_CurrentAnimationIndex == -1) return true;
	if (m_Model->Animations.empty()) return true;
	const Animation& animCurrent = m_Model->Animations[m_CurrentAnimationIndex];
	if (m_Loop) return false; // 循环播放永远不算完成
	return m_CurrentTime >= animCurrent.duration;
}

bool Animator::OnCurrAniStarted() const
{
	if (!m_Model || m_CurrentAnimationIndex == -1) return false;
	if (m_Model->Animations.empty()) return false;
	return m_CurrentTime == 0.0;
}

float Animator::GetCurrAniProgress() const
{
	if (!m_Model || m_CurrentAnimationIndex == -1) return 0.0f;
	if (m_Model->Animations.empty()) return 0.0f;
	const Animation& animCurrent = m_Model->Animations[m_CurrentAnimationIndex];
	if (animCurrent.duration <= 0.0) return 0.0f;
	return static_cast<float>(m_CurrentTime / animCurrent.duration);
}

void Animator::Update(double elapsedTime)
{
	if (!m_Model || m_CurrentAnimationIndex == -1) return;
	if (m_Model->Animations.empty()) return;

	double scaledTime = elapsedTime * m_Speed;

	// Update Current Animation Time
	const Animation& animCurrent = m_Model->Animations[m_CurrentAnimationIndex];
	m_CurrentTime += scaledTime * animCurrent.ticksPerSecond;
	
	if (m_Loop) m_CurrentTime = fmod(m_CurrentTime, animCurrent.duration);
	else if (m_CurrentTime >= animCurrent.duration) m_CurrentTime = animCurrent.duration;

	// Update blend transition timer (snapshot doesn't need time advancement)
	if (m_IsBlending)
	{
		m_TransitionTime += scaledTime; // Real time
		if (m_TransitionTime >= m_TransitionDuration)
		{
			m_IsBlending = false;
			m_TransitionTime = 0.0;
		}
	}

	// Update Additive Layer
	if (m_AdditiveAnimIndex != -1)
	{
		const Animation& addAnim = m_Model->Animations[m_AdditiveAnimIndex];
		m_AdditiveTime += scaledTime * addAnim.ticksPerSecond;

		if (m_AdditiveLoop)
		{
			m_AdditiveTime = fmod(m_AdditiveTime, addAnim.duration);
		}
		else if (m_AdditiveTime >= addAnim.duration)
		{
			m_AdditiveTime = addAnim.duration;
			// One-shot additive finished — auto stop
			if (!m_AdditiveFadingOut)
				StopAdditive(0.1);
		}

		// Fade-out processing
		if (m_AdditiveFadingOut)
		{
			m_AdditiveFadeTimer += scaledTime;
			if (m_AdditiveFadeTimer >= m_AdditiveFadeDuration)
			{
				// Fade complete
				m_AdditiveAnimIndex = -1;
				m_AdditiveWeight = 0.0f;
				m_AdditiveFadingOut = false;
			}
			else
			{
				float fadeProgress = (float)(m_AdditiveFadeTimer / m_AdditiveFadeDuration);
				m_AdditiveWeight = m_AdditiveTargetWeight * (1.0f - fadeProgress);
			}
		}
	}

	// Calculate transformations
	for (int i = 0; i < (int)m_Model->Bones.size(); ++i)
	{
		if (m_Model->Bones[i].parentIndex == -1)
		{
			UpdateGlobalTransforms(i, XMMatrixIdentity(), animCurrent);
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

void Animator::GetBoneSRT(int boneIndex, const Animation& anim, double time, XMVECTOR& outS, XMVECTOR& outR, XMVECTOR& outT) const
{
	const Bone& boneDef = m_Model->Bones[boneIndex];
	
	// Default to local matrix components (Bind Pose)
	XMMATRIX nodeTransform = XMLoadFloat4x4(&boneDef.localMatrix);
	XMVECTOR s, r, t;
	if (!XMMatrixDecompose(&s, &r, &t, nodeTransform)) {
		// Fallback
		s = XMVectorSet(1,1,1,1);
		r = XMQuaternionIdentity();
		t = XMVectorSet(0,0,0,1);
	}

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
		if (!channel->positionKeys.empty()) {
			int idx = FindKeyIndex(time, channel->positionKeys);
			int nextIdx = (idx + 1) % channel->positionKeys.size();
			double t1 = channel->positionKeys[idx].time;
			double t2 = channel->positionKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((time - t1) / dt);
			if (dt == 0) factor = 0;
			
			XMVECTOR p1 = XMLoadFloat3(&channel->positionKeys[idx].value);
			XMVECTOR p2 = XMLoadFloat3(&channel->positionKeys[nextIdx].value);
			t = XMVectorLerp(p1, p2, factor);
		}
		
		// Interpolate Rotation
		if (!channel->rotationKeys.empty()) {
			int idx = FindKeyIndex(time, channel->rotationKeys);
			int nextIdx = (idx + 1) % channel->rotationKeys.size();
			double t1 = channel->rotationKeys[idx].time;
			double t2 = channel->rotationKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((time - t1) / dt);
			if (dt == 0) factor = 0;

			// Handle different Rotation representations (Quaternions usually)
			XMVECTOR r1 = XMLoadFloat4(&channel->rotationKeys[idx].value);
			XMVECTOR r2 = XMLoadFloat4(&channel->rotationKeys[nextIdx].value);
			r = XMQuaternionSlerp(r1, r2, factor);
		}
		
		// Interpolate Scale
		if (!channel->scalingKeys.empty()) {
			int idx = FindKeyIndex(time, channel->scalingKeys);
			int nextIdx = (idx + 1) % channel->scalingKeys.size();
			double t1 = channel->scalingKeys[idx].time;
			double t2 = channel->scalingKeys[nextIdx].time;
			double dt = t2 - t1;
			if (dt < 0) dt += anim.duration;
			float factor = (float)((time - t1) / dt);
			if (dt == 0) factor = 0;

			XMVECTOR s1 = XMLoadFloat3(&channel->scalingKeys[idx].value);
			XMVECTOR s2 = XMLoadFloat3(&channel->scalingKeys[nextIdx].value);
			s = XMVectorLerp(s1, s2, factor);
		}
	}
	
	outS = s;
	outR = r;
	outT = t;
}

void Animator::UpdateGlobalTransforms(int boneIndex, const DirectX::XMMATRIX& parentTransform, const Animation& anim)
{
	const Bone& boneDef = m_Model->Bones[boneIndex];
	
	XMVECTOR s, r, t;

	// Calculate Current Animation Local Transform
	GetBoneSRT(boneIndex, anim, m_CurrentTime, s, r, t);

	if (m_IsBlending)
	{
		// 从快照中取出过渡前的姿态
		XMVECTOR prevS = XMLoadFloat4(&m_SnapshotPose[boneIndex].scale);
		XMVECTOR prevR = XMLoadFloat4(&m_SnapshotPose[boneIndex].rotation);
		XMVECTOR prevT = XMLoadFloat4(&m_SnapshotPose[boneIndex].translation);
		
		float factor = (float)(m_TransitionTime / m_TransitionDuration);
		factor = std::max(0.0f, std::min(1.0f, factor));
		
		s = XMVectorLerp(prevS, s, factor);
		r = XMQuaternionSlerp(prevR, r, factor);
		t = XMVectorLerp(prevT, t, factor);
	}

	// Apply Additive Layer
	if (m_AdditiveAnimIndex != -1 && m_AdditiveWeight > 0.0f)
	{
		ApplyAdditive(boneIndex, s, r, t);
	}
	
	XMMATRIX localTransform = XMMatrixScalingFromVector(s) * XMMatrixRotationQuaternion(r) * XMMatrixTranslationFromVector(t);
	XMMATRIX globalTransform = localTransform * parentTransform;
	
	XMStoreFloat4x4(&m_GlobalMatrices[boneIndex], globalTransform);

	// Final Matrix
	XMMATRIX offset = XMLoadFloat4x4(&boneDef.offsetMatrix);
	XMMATRIX finalM = offset * globalTransform;
	XMStoreFloat4x4(&m_FinalBoneMatrices[boneIndex], finalM);

	for (int childIndex : boneDef.children)
	{
		UpdateGlobalTransforms(childIndex, globalTransform, anim);
	}
}

const std::vector<DirectX::XMFLOAT4X4>& Animator::GetFinalBoneMatrices() const
{
	return m_FinalBoneMatrices;
}

const DirectX::XMFLOAT4X4& Animator::GetBoneGlobalMatrix(int boneIndex) const
{
	return m_GlobalMatrices[boneIndex];
}

void Animator::TakeSnapshot()
{
	if (!m_Model) return;

	const Animation& currAnim = m_Model->Animations[m_CurrentAnimationIndex];

	for (int i = 0; i < (int)m_Model->Bones.size(); ++i)
	{
		XMVECTOR s, r, t;
		GetBoneSRT(i, currAnim, m_CurrentTime, s, r, t);

		// 如果正在混合中，需要混入快照产生「真实视觉姿态」
		if (m_IsBlending)
		{
			XMVECTOR snapS = XMLoadFloat4(&m_SnapshotPose[i].scale);
			XMVECTOR snapR = XMLoadFloat4(&m_SnapshotPose[i].rotation);
			XMVECTOR snapT = XMLoadFloat4(&m_SnapshotPose[i].translation);

			float factor = (float)(m_TransitionTime / m_TransitionDuration);
			factor = std::max(0.0f, std::min(1.0f, factor));

			s = XMVectorLerp(snapS, s, factor);
			r = XMQuaternionSlerp(snapR, r, factor);
			t = XMVectorLerp(snapT, t, factor);
		}

		XMStoreFloat4(&m_SnapshotPose[i].scale, s);
		XMStoreFloat4(&m_SnapshotPose[i].rotation, r);
		XMStoreFloat4(&m_SnapshotPose[i].translation, t);
	}
}

//=============================================================================
// Additive Layer
//=============================================================================
void Animator::PlayAdditive(const std::string& name, bool loop, float weight)
{
	if (!m_Model) return;
	for (int i = 0; i < (int)m_Model->Animations.size(); ++i)
	{
		if (m_Model->Animations[i].name == name)
		{
			PlayAdditive(i, loop, weight);
			return;
		}
	}
}

void Animator::PlayAdditive(int index, bool loop, float weight)
{
	if (!m_Model) return;
	if (index < 0 || index >= (int)m_Model->Animations.size()) return;

	m_AdditiveAnimIndex = index;
	m_AdditiveTime = 0.0;
	m_AdditiveLoop = loop;
	m_AdditiveWeight = weight;
	m_AdditiveTargetWeight = weight;
	m_AdditiveFadingOut = false;
}

void Animator::StopAdditive(double fadeOutTime)
{
	if (m_AdditiveAnimIndex == -1) return;

	if (fadeOutTime <= 0.0)
	{
		m_AdditiveAnimIndex = -1;
		m_AdditiveWeight = 0.0f;
		m_AdditiveFadingOut = false;
	}
	else
	{
		m_AdditiveFadingOut = true;
		m_AdditiveFadeTimer = 0.0;
		m_AdditiveFadeDuration = fadeOutTime;
		m_AdditiveTargetWeight = m_AdditiveWeight; // fade from current weight
	}
}

void Animator::ApplyAdditive(int boneIndex, XMVECTOR& s, XMVECTOR& r, XMVECTOR& t)
{
	const Animation& addAnim = m_Model->Animations[m_AdditiveAnimIndex];

	// Sample current frame of additive animation
	XMVECTOR addS, addR, addT;
	GetBoneSRT(boneIndex, addAnim, m_AdditiveTime, addS, addR, addT);

	// Sample reference pose (frame 0) of additive animation
	XMVECTOR refS, refR, refT;
	GetBoneSRT(boneIndex, addAnim, 0.0, refS, refR, refT);

	// Compute additive delta
	XMVECTOR deltaT = XMVectorSubtract(addT, refT);
	XMVECTOR deltaR = XMQuaternionMultiply(addR, XMQuaternionInverse(refR));

	// Apply with weight
	float w = m_AdditiveWeight;
	t = XMVectorAdd(t, XMVectorScale(deltaT, w));
	XMVECTOR weightedR = XMQuaternionSlerp(XMQuaternionIdentity(), deltaR, w);
	r = XMQuaternionMultiply(weightedR, r);
	// Scale: keep base scale unchanged
}
