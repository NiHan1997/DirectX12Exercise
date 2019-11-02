#include "SkinnedData.h"

Keyframe::Keyframe() : TimePos(0.0f),
Translation(0.0f, 0.0f, 0.0f),
Scale(1.0f, 1.0f, 1.0f),
RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)
{
}

Keyframe::~Keyframe()
{
}

float BoneAnimation::GetStartTime() const
{
	return Keyframes.front().TimePos;
}

float BoneAnimation::GetEndTime() const
{
	return Keyframes.back().TimePos;
}

void BoneAnimation::Interpolate(float time, OUT XMFLOAT4X4& matrixf4) const
{
	if (time < Keyframes.front().TimePos)
	{
		XMVECTOR scale = XMLoadFloat3(&Keyframes.front().Scale);
		XMVECTOR translation = XMLoadFloat3(&Keyframes.front().Translation);
		XMVECTOR rotation = XMLoadFloat4(&Keyframes.front().RotationQuat);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&matrixf4, XMMatrixAffineTransformation(scale, zero, rotation, translation));
	}
	else if (time > Keyframes.back().TimePos)
	{
		XMVECTOR scale = XMLoadFloat3(&Keyframes.back().Scale);
		XMVECTOR translation = XMLoadFloat3(&Keyframes.back().Translation);
		XMVECTOR rotation = XMLoadFloat4(&Keyframes.back().RotationQuat);

		XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		XMStoreFloat4x4(&matrixf4, XMMatrixAffineTransformation(scale, zero, rotation, translation));
	}
	else
	{
		for (int i = 0; i < (int)Keyframes.size() - 1; ++i)
		{
			if (time >= Keyframes[i].TimePos && time <= Keyframes[i + 1].TimePos)
			{
				float lerpPercent = (time - Keyframes[i].TimePos) / (Keyframes[i + 1].TimePos - Keyframes[i].TimePos);

				XMVECTOR scale0 = XMLoadFloat3(&Keyframes[i].Scale);
				XMVECTOR scale1 = XMLoadFloat3(&Keyframes[i + 1].Scale);

				XMVECTOR translation0 = XMLoadFloat3(&Keyframes[i].Translation);
				XMVECTOR translation1 = XMLoadFloat3(&Keyframes[i + 1].Translation);

				XMVECTOR rotation0 = XMLoadFloat4(&Keyframes[i].RotationQuat);
				XMVECTOR rotation1 = XMLoadFloat4(&Keyframes[i + 1].RotationQuat);

				XMVECTOR scale = XMVectorLerp(scale0, scale1, lerpPercent);
				XMVECTOR translation = XMVectorLerp(translation0, translation1, lerpPercent);
				XMVECTOR rotation = XMQuaternionSlerp(rotation0, rotation1, lerpPercent);

				XMVECTOR zero = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
				XMStoreFloat4x4(&matrixf4, XMMatrixAffineTransformation(scale, zero, rotation, translation));

				break;
			}
		}
	}
}

float AnimationClip::GetClipStartTime() const
{
	float time = MathHelper::Infinity;
	for (UINT i = 0; i < (UINT)BoneAnimations.size(); ++i)
	{
		time = MathHelper::Min(time, BoneAnimations[i].GetStartTime());
	}

	return time;
}

float AnimationClip::GetCilpEndTime() const
{
	float time = 0.0f;
	for (UINT i = 0; i < (UINT)BoneAnimations.size(); ++i)
	{
		time = MathHelper::Max(time, BoneAnimations[i].GetEndTime());
	}

	return time;
}

void AnimationClip::Interpolate(float time, OUT std::vector<XMFLOAT4X4>& boneTransforms) const
{
	for (UINT i = 0; i < (UINT)BoneAnimations.size(); ++i)
	{
		BoneAnimations[i].Interpolate(time, boneTransforms[i]);
	}
}

UINT SkinnedData::BoneCount() const
{
	return (UINT)mBoneHierarchy.size();
}

float SkinnedData::GetClipStartTime(std::string clipName) const
{
	return mAnimations.find(clipName)->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(std::string clipName) const
{
	return mAnimations.find(clipName)->second.GetCilpEndTime();;
}

void SkinnedData::Set(OUT std::vector<int>& boneHierachy,
	OUT std::vector<XMFLOAT4X4>& boneOffsets,
	OUT std::unordered_map<std::string, AnimationClip>& animations)
{
	mBoneHierarchy = boneHierachy;
	mBoneOffsets = boneOffsets;
	mAnimations = animations;
}

void SkinnedData::GetFinalTransforms(const std::string clipName, float timePos,
	OUT std::vector<XMFLOAT4X4>& finalTransforms) const
{
	UINT numBones = (UINT)mBoneOffsets.size();
	std::vector<XMFLOAT4X4> toParentTransforms(numBones);

	// 获取 节点-->父节点 的变换.
	mAnimations.find(clipName)->second.Interpolate(timePos, toParentTransforms);

	// 遍历骨骼, 获取 节点-->根坐标 的变换.
	std::vector<XMFLOAT4X4> toRootTransforms(numBones);
	toRootTransforms[0] = toParentTransforms[0];
	for (UINT i = 1; i < numBones; ++i)
	{
		XMMATRIX toParent = XMLoadFloat4x4(&toParentTransforms[i]);

		// 获取父节点变换到根坐标的矩阵.
		int parentIndex = mBoneHierarchy[i];
		XMMATRIX parentToRoot = XMLoadFloat4x4(&toRootTransforms[parentIndex]);

		// 节点-->根坐标.
		XMMATRIX toRoot = XMMatrixMultiply(toParent, parentToRoot);
		XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	// 绑定空间 --> 偏移变换 --> 根坐标变换 == 最终变换.
	for (UINT i = 0; i < numBones; ++i)
	{
		XMMATRIX offset = XMLoadFloat4x4(&mBoneOffsets[i]);
		XMMATRIX toRoot = XMLoadFloat4x4(&toRootTransforms[i]);
		XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
		XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}
