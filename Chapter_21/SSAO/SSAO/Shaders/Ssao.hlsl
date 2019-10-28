///***********************************************************************************
/// 绘制SSAO纹理的着色器.
///***********************************************************************************

/// 计算SSAO需要的常量缓冲区数据.
struct SSAOPassCB
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4 gOffsetVectors[14];

	// SsaoBlur.hlsl使用的数据, 在这里只是共用根签名.
	float4 gBlurWeights[3];

	float2 gInvRenderTargetSize;

	// 这些数值都是在观察空间计算的.
	float gOcclusionRadius;			// p为圆心半球的范围.
	float gOcclusionFadeStart;		// 计算SSAO遮蔽的范围.
	float gOcclusionFadeEnd;
	float gSurfaceEpsilon;			// 如果两个点距离过小, 有可能造成自相交.
};

/// 在这里不使用, 在模糊SSAO纹理的时候使用.
struct BlurDir
{
	bool gHorizontalBlur;
};

ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);
ConstantBuffer<BlurDir> gBlurDirCB     : register(b1);

/// 法线纹理、深度纹理、随机向量纹理.
Texture2D gNormalMap       : register(t0);
Texture2D gDepthMap        : register(t1);
Texture2D gRandomVectorMap : register(t2);

/// 可能会使用到的常用的静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamDepthMap		  : register(s7);

// 计算SSAO的采样次数, 次数越多效果肯定越好, 性能消耗也越高.
static const int gSampleCount = 14;

/// 基于屏幕后处理实现需要的顶点(索引).
static const float2 gTexCoords[6] =
{
	float2(0.0, 1.0),
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 0.0),
	float2(1.0, 1.0)
};

/// 顶点着色器输入.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

/// 顶点着色器.
VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	vout.TexC = gTexCoords[vid];

	// 将屏幕后处理后的纹理显示到NDC空间的近平面上.
	// 屏幕后处理的本质就是在原始后台缓冲区的基础上绘制一个四边形呈现在屏幕上.
	vout.PosH = float4(2 * vout.TexC.x - 1.0, 1.0 - 2 * vout.TexC.y, 0.0, 1.0);

	// 从NDC空间的近平面变换到观察空间的近平面.
	float4 posV = mul(vout.PosH, gSSAOPassCB.gInvProj);
	vout.PosV = posV.xyz / posV.w;

	return vout;
}

// 确定目标点p被q的遮挡程度.
// distZ = p.z - r.z
// q是p半球范围的随机一点, r是观察空间中可能遮蔽p的可视点.
float OcclusionFunction(float distZ)
{
	float occlusion = 0.0;
	if (distZ > gSSAOPassCB.gSurfaceEpsilon)
	{
		float fadeLength = gSSAOPassCB.gOcclusionFadeEnd - gSSAOPassCB.gOcclusionFadeStart;
		occlusion = saturate((gSSAOPassCB.gOcclusionFadeEnd - distZ) / fadeLength);
	}

	return occlusion;
}

/// NDC深度值-->观察空间线性深度值.
float NdcDepthToViewDepth(float z_ndc)
{
	// z_ndc = A + B / viewZ, 其中A = gProj[2][2], B = gProj[3][2].
	float viewZ = gSSAOPassCB.gProj[3][2] / (z_ndc - gSSAOPassCB.gProj[2][2]);
	return viewZ;
}

/// 像素着色器.
float4 PS(VertexOut pin) : SV_Target
{
	/// 数据说明:
	/// p : 需要计算SSAO的目标点.
	/// n : 点p位置的观察空间法向量.
	/// q : 随机偏离q的一点.
	/// r : 可能遮蔽p的点.

	// 获取对应像素的观察空间的法线和深度.
	float3 n = gNormalMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).xyz;
	float pz = gDepthMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).r;
	pz = NdcDepthToViewDepth(pz);

	/// 重构观察空间的位置.
	/// p = t * pin.PosV
	/// p.z = t * pin.PosV.z
	/// t = p.z / pin.PosV.z
	float3 p = (pz / pin.PosV.z) * pin.PosV;

	// 获取随机向量, 纹理是[0, 1], 我们需要映射到[-1, 1].
	float3 randVec = 2.0 * gRandomVectorMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).rgb - 1.0;

	// 以p为球心, 对p临近范围采样.
	float occlusionSum = 0.0;
	for (int i = 0; i < gSampleCount; ++i)
	{
		float3 offset = reflect(gSSAOPassCB.gOffsetVectors[i].xyz, randVec);

		// 采集获取点q.
		float filp = sign(dot(offset, n));
		float3 q = p + filp * gSSAOPassCB.gOcclusionRadius * offset;

		// 获取点q的纹理坐标.
		float4 projQ = mul(float4(q, 1.0), gSSAOPassCB.gProjTex);
		projQ /= projQ.w;

		// 采样获取观察点到q中最近的可视点r.
		float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0).r;
		rz = NdcDepthToViewDepth(rz);
		float3 r = (rz / q.z) * q;

		// 计算r是否是遮蔽点.
		float distZ = p.z - r.z;
		float dp = max(0, dot(n, normalize(r - p)));
		float occlusion = dp * OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}

	occlusionSum /= gSampleCount;
	float access = 1.0 - occlusionSum;

	// 提升对比度.
	return saturate(pow(access, 8.0));
}