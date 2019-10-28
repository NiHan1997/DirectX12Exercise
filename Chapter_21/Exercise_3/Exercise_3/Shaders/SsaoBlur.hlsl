///***********************************************************************************
/// 对SSAO纹理进行模糊运算的着色器.
/// 在这里使用的是双边模糊, 这种方式和高斯模糊相似, 但是能更好的保留图像的边缘,
/// 在Chapter 13习题中已经初步使用, 在这里配合深度、法线纹理能够更好地确定边界,
/// 这也是边缘检测的一种思路.
///***********************************************************************************

/// 计算SSAO需要的常量缓冲区数据.
struct SSAOPassCB
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4 gOffsetVectors[14];

	// 模糊权重.
	float4 gBlurWeights[3];

	float2 gInvRenderTargetSize;

	// 这些数值都是在观察空间计算的.
	float gOcclusionRadius;			// p为圆心半球的范围.
	float gOcclusionFadeStart;		// 计算SSAO遮蔽的范围.
	float gOcclusionFadeEnd;
	float gSurfaceEpsilon;			// 如果两个点距离过小, 有可能造成自相交.
};

/// SSAO过程常量缓冲区.
ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);

/// 法线纹理、深度纹理、SSAO输入纹理.
Texture2D gNormalMap : register(t0);
Texture2D gDepthMap  : register(t1);
Texture2D gInputMap  : register(t2);

/// 模糊完成后的纹理.
RWTexture2D<float4> gOutputMap : register(u0);

/// 可能会使用到的常用的静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamDepthMap		  : register(s7);

/// 模糊半径.
static const int gBlurRadius = 5;

#define N 256
#define CacheSize (N + 2 * gBlurRadius)

// 共享内存, 包含颜色、法线、深度.
groupshared float4 gColorCache[CacheSize];
groupshared float3 gNormalCache[CacheSize];
groupshared float gDepthCache[CacheSize];

/// NDC深度值-->观察空间线性深度值.
float NdcDepthToViewDepth(float z_ndc)
{
	// z_ndc = A + B / viewZ, 其中A = gProj[2][2], B = gProj[3][2].
	float viewZ = gSSAOPassCB.gProj[3][2] / (z_ndc - gSSAOPassCB.gProj[2][2]);
	return viewZ;
}

/// 横向模糊操作的计算着色器.
[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	// 模糊所需要的权重值, 这里实际上只会使用11个.
	float blurWeights[12] =
	{
		gSSAOPassCB.gBlurWeights[0].x, gSSAOPassCB.gBlurWeights[0].y,
		gSSAOPassCB.gBlurWeights[0].z, gSSAOPassCB.gBlurWeights[0].w,

		gSSAOPassCB.gBlurWeights[1].x, gSSAOPassCB.gBlurWeights[1].y,
		gSSAOPassCB.gBlurWeights[1].z, gSSAOPassCB.gBlurWeights[1].w,

		gSSAOPassCB.gBlurWeights[2].x, gSSAOPassCB.gBlurWeights[2].y,
		gSSAOPassCB.gBlurWeights[2].z, gSSAOPassCB.gBlurWeights[2].w
	};

	// 共享内存左边区域填充.
	if (groupThreadID.x < gBlurRadius)
	{
		int x = max(0, dispatchThreadID.x - gBlurRadius);

		gColorCache[groupThreadID.x] = gInputMap[int2(x, dispatchThreadID.y)];
		gNormalCache[groupThreadID.x] = gNormalMap[int2(x, dispatchThreadID.y)].rgb;
		gDepthCache[groupThreadID.x] = gDepthMap[int2(x, dispatchThreadID.y)].r;
	}

	// 共享内存右边区域填充.
	if (groupThreadID.x >= N - gBlurRadius)
	{
		int x = min(gInputMap.Length.x - 1, dispatchThreadID.x + gBlurRadius);

		gColorCache[groupThreadID.x + 2 * gBlurRadius] = gInputMap[int2(x, dispatchThreadID.y)];
		gNormalCache[groupThreadID.x + 2 * gBlurRadius] = gNormalMap[int2(x, dispatchThreadID.y)].rgb;
		gDepthCache[groupThreadID.x + 2 * gBlurRadius] = gDepthMap[int2(x, dispatchThreadID.y)].r;
	}

	// 中央区域共享内存填充.
	gColorCache[groupThreadID.x + gBlurRadius] = gInputMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)];
	gNormalCache[groupThreadID.x + gBlurRadius] = gNormalMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].rgb;
	gDepthCache[groupThreadID.x + gBlurRadius] = gDepthMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].r;

	GroupMemoryBarrierWithGroupSync();

	// 中央像素采集.
	float totalWeight = blurWeights[gBlurRadius];
	float4 color = totalWeight * gColorCache[groupThreadID.x + gBlurRadius];
	float3 centerNormal = gNormalCache[groupThreadID.x + gBlurRadius];
	float centerDepth = NdcDepthToViewDepth(gDepthCache[groupThreadID.x + gBlurRadius]);

	// 逐像素处理.	
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		if (i == 0)
			continue;

		// 临近像素采样.
		int texIndex = groupThreadID.x + gBlurRadius + i;
		float3 neighborNormal = gNormalCache[texIndex];
		float neighborDepth = NdcDepthToViewDepth(gDepthCache[texIndex]);

		if (dot(neighborNormal, centerNormal) >= 0.8 &&
			abs(neighborDepth - neighborDepth) <= 0.2)
		{
			float weight = blurWeights[i + gBlurRadius];
			color += weight * gColorCache[texIndex];
			totalWeight += weight;
		}
	}

	gOutputMap[dispatchThreadID.xy] = color / totalWeight;
}

/// 纵向模糊操作的计算着色器.
[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	// 模糊所需要的权重值, 这里实际上只会使用11个.
	float blurWeights[12] =
	{
		gSSAOPassCB.gBlurWeights[0].x, gSSAOPassCB.gBlurWeights[0].y,
		gSSAOPassCB.gBlurWeights[0].z, gSSAOPassCB.gBlurWeights[0].w,

		gSSAOPassCB.gBlurWeights[1].x, gSSAOPassCB.gBlurWeights[1].y,
		gSSAOPassCB.gBlurWeights[1].z, gSSAOPassCB.gBlurWeights[1].w,

		gSSAOPassCB.gBlurWeights[2].x, gSSAOPassCB.gBlurWeights[2].y,
		gSSAOPassCB.gBlurWeights[2].z, gSSAOPassCB.gBlurWeights[2].w
	};

	// 共享内存上边区域填充.
	if (groupThreadID.y < gBlurRadius)
	{
		int y = max(0, dispatchThreadID.y - gBlurRadius);

		gColorCache[groupThreadID.y] = gInputMap[int2(dispatchThreadID.x, y)];
		gNormalCache[groupThreadID.y] = gNormalMap[int2(dispatchThreadID.x, y)].rgb;
		gDepthCache[groupThreadID.y] = gDepthMap[int2(dispatchThreadID.x, y)].r;
	}

	// 共享内存下边区域填充.
	if (groupThreadID.y >= N - gBlurRadius)
	{
		int y = min(gInputMap.Length.y - 1, dispatchThreadID.y + gBlurRadius);

		gColorCache[groupThreadID.y + 2 * gBlurRadius] = gInputMap[int2(dispatchThreadID.x, y)];
		gNormalCache[groupThreadID.y + 2 * gBlurRadius] = gNormalMap[int2(dispatchThreadID.x, y)].rgb;
		gDepthCache[groupThreadID.y + 2 * gBlurRadius] = gDepthMap[int2(dispatchThreadID.x, y)].r;
	}

	// 中央区域共享内存填充.
	gColorCache[groupThreadID.y + gBlurRadius] = gInputMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)];
	gNormalCache[groupThreadID.y + gBlurRadius] = gNormalMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].rgb;
	gDepthCache[groupThreadID.y + gBlurRadius] = gDepthMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].r;

	GroupMemoryBarrierWithGroupSync();

	// 中央像素采集.
	float totalWeight = blurWeights[gBlurRadius];
	float4 color = totalWeight * gColorCache[groupThreadID.y + gBlurRadius];
	float3 centerNormal = gNormalCache[groupThreadID.y + gBlurRadius];
	float centerDepth = NdcDepthToViewDepth(gDepthCache[groupThreadID.y + gBlurRadius]);

	// 逐像素处理.
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		if (i == 0)
			continue;

		// 临近像素采样.
		int texIndex = groupThreadID.y + gBlurRadius + i;
		float3 neighborNormal = gNormalCache[texIndex];
		float neighborDepth = NdcDepthToViewDepth(gDepthCache[texIndex]);

		if (dot(neighborNormal, centerNormal) >= 0.8 &&
			abs(neighborDepth - neighborDepth) <= 0.2)
		{
			float weight = blurWeights[i + gBlurRadius];
			color += weight * gColorCache[texIndex];
			totalWeight += weight;
		}
	}

	gOutputMap[dispatchThreadID.xy] = color / totalWeight;
}