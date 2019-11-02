#pragma once

#include "..\..\..\Common\d3dUtil.h"

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

	void Interpolate(float time, XMFLOAT4X4& matrixf4) const;

	std::vector<Keyframe> Keyframes;
};
