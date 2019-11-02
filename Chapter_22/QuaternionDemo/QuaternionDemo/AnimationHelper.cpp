#include "AnimationHelper.h"

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

void BoneAnimation::Interpolate(float time, DirectX::XMFLOAT4X4& matrixf4) const
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
