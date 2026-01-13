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
	, m_PrevAnimationIndex(-1), m_PrevTime(0.0), m_PrevLoop(false)
	, m_IsBlending(false), m_BlendFactor(0.0), m_TransitionTime(0.0), m_TransitionDuration(0.0)
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
			// -------------------------------------------------------------
			// 【防卡死改进】：如果当前已经在进行 "同动画混合" (A -> A)，且混合未结束，
			// 则忽略新的重播请求。避免在 Update 中每帧调用导致无限重置在第 0 帧。
			// -------------------------------------------------------------
			// if (m_IsBlending && m_PrevAnimationIndex == index) ... REMOVED

			if (blendTime > 0.0)
			{
				m_PrevAnimationIndex = m_CurrentAnimationIndex;
				m_PrevTime = m_CurrentTime;
				m_PrevLoop = m_Loop;

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
		// ---------------------------------------------------------
		// 【核心修复】：检测是否切回了正在淡出的动画 (A -> B -> A)
		// ---------------------------------------------------------
		if (m_IsBlending && index == m_PrevAnimationIndex && m_PrevLoop)
		{
			// 此时：Prev 是 A, Curr 是 B, 进度走了 30%
			// 目标：切回 A

			// 1. 交换 Current 和 Prev 的身份
			std::swap(m_CurrentAnimationIndex, m_PrevAnimationIndex);
			std::swap(m_CurrentTime, m_PrevTime);
			std::swap(m_Loop, m_PrevLoop);

			// 2. 关键：反转混合进度
			// 现在的进度是 "从 Prev 到 Curr" (比如走了 0.3s)
			// 交换后，视觉上我们需要 "从 新Prev 到 新Curr" 走了 0.7s 的位置
			// 也就是倒着走回去

			// 计算当前的归一化进度 (0.0 ~ 1.0)
			double currentFactor = 0.0;
			if (m_TransitionDuration > 0.0) {
				currentFactor = m_TransitionTime / m_TransitionDuration;
			}

			// 反转进度 (1.0 - 0.3 = 0.7)
			double invertedFactor = 1.0 - currentFactor;

			// 重新设置新的过渡参数
			m_TransitionDuration = blendTime; // 使用新的混合时间
			m_TransitionTime = invertedFactor * m_TransitionDuration; // 映射回时间

			// 保持混合状态为 true
			m_IsBlending = true;

			return; // 搞定，直接退出
		}

		// ---------------------------------------------------------
		// 正常流程：开启一个新的混合 (A -> C)
		// ---------------------------------------------------------
		if (blendTime > 0.0 && m_CurrentAnimationIndex != -1)
		{
			m_PrevAnimationIndex = m_CurrentAnimationIndex;
			m_PrevTime = m_CurrentTime;
			m_PrevLoop = m_Loop;

			m_CurrentAnimationIndex = index;
			m_CurrentTime = 0.0; // 新动画从头开始
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

	// Update Previous Animation Time (if blending)
	if (m_IsBlending)
	{
		const Animation& animPrev = m_Model->Animations[m_PrevAnimationIndex];
		m_PrevTime += scaledTime * animPrev.ticksPerSecond;
		
		if (m_PrevLoop) m_PrevTime = fmod(m_PrevTime, animPrev.duration);
		else if (m_PrevTime >= animPrev.duration) m_PrevTime = animPrev.duration;
		
		m_TransitionTime += scaledTime; // Real time
		if (m_TransitionTime >= m_TransitionDuration)
		{
			m_IsBlending = false;
			m_TransitionTime = 0.0;
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
		const Animation& prevAnim = m_Model->Animations[m_PrevAnimationIndex];
		XMVECTOR prevS, prevR, prevT;
		GetBoneSRT(boneIndex, prevAnim, m_PrevTime, prevS, prevR, prevT);
		
		float factor = (float)(m_TransitionTime / m_TransitionDuration);
		factor = std::max(0.0f, std::min(1.0f, factor));
		
		s = XMVectorLerp(prevS, s, factor);
		r = XMQuaternionSlerp(prevR, r, factor);
		t = XMVectorLerp(prevT, t, factor);
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
