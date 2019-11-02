#pragma once

#include "..\..\..\Common\d3dUtil.h"

#define OUT

using namespace DirectX;

///<summary>
/// 关键帧, 相当于所有的动画效果就是在关键帧直接插值.
///</summary>
struct Keyframe
{
	Keyframe();
	~Keyframe();

	float TimePos = 0.0f;
	XMFLOAT3 Translation;
	XMFLOAT3 Scale;
	XMFLOAT4 RotationQuat;
};

///<summary>
/// 动画效果就是在两个关键帧直接插值, 因此需要存储所有的关键帧, 并时刻注意现在动画是处于哪两个关键帧之中.
///</summary>
struct BoneAnimation
{
	float GetStartTime() const;
	float GetEndTime() const;

	void Interpolate(float time, OUT XMFLOAT4X4& matrixf4) const;

	std::vector<Keyframe> Keyframes;
};

///<summary>
/// 一个Cilp就是一个动作, 一个角色可以有多个动作, 每一个动作的变化都是不同的.
///</summary>
struct AnimationClip
{
	float GetClipStartTime() const;
	float GetCilpEndTime() const;

	void Interpolate(float time, OUT std::vector<XMFLOAT4X4>& boneTransforms) const;

	std::vector<BoneAnimation> BoneAnimations;
};

class SkinnedData
{
public:
	UINT BoneCount() const;

	float GetClipStartTime(std::string clipName) const;
	float GetClipEndTime(std::string clipName) const;

	void Set(OUT std::vector<int>& boneHierachy,
		OUT std::vector<XMFLOAT4X4>& boneOffsets,
		OUT std::unordered_map<std::string, AnimationClip>& animations);

	void GetFinalTransforms(const std::string clipName, float timePos,
		OUT std::vector<XMFLOAT4X4>& finalTransforms) const;

private:
	// 角色的骨骼结构索引.
	std::vector<int> mBoneHierarchy;

	// 骨骼相当于根节点的偏移量.
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

	// 动作存储.
	std::unordered_map<std::string, AnimationClip> mAnimations;
};
