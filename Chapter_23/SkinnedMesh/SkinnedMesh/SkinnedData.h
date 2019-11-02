#pragma once

#include "..\..\..\Common\d3dUtil.h"

#define OUT

using namespace DirectX;

///<summary>
/// �ؼ�֡, �൱�����еĶ���Ч�������ڹؼ�ֱ֡�Ӳ�ֵ.
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
/// ����Ч�������������ؼ�ֱ֡�Ӳ�ֵ, �����Ҫ�洢���еĹؼ�֡, ��ʱ��ע�����ڶ����Ǵ����������ؼ�֮֡��.
///</summary>
struct BoneAnimation
{
	float GetStartTime() const;
	float GetEndTime() const;

	void Interpolate(float time, OUT XMFLOAT4X4& matrixf4) const;

	std::vector<Keyframe> Keyframes;
};

///<summary>
/// һ��Cilp����һ������, һ����ɫ�����ж������, ÿһ�������ı仯���ǲ�ͬ��.
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
	// ��ɫ�Ĺ����ṹ����.
	std::vector<int> mBoneHierarchy;

	// �����൱�ڸ��ڵ��ƫ����.
	std::vector<DirectX::XMFLOAT4X4> mBoneOffsets;

	// �����洢.
	std::unordered_map<std::string, AnimationClip> mAnimations;
};
