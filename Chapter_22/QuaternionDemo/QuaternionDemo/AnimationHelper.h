#pragma once

#include "..\..\..\Common\d3dUtil.h"

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

	void Interpolate(float time, XMFLOAT4X4& matrixf4) const;

	std::vector<Keyframe> Keyframes;
};
