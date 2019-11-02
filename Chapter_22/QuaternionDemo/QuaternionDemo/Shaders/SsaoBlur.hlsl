///***********************************************************************************
/// ��SSAO�������ģ���������ɫ��.
/// ������ʹ�õ���˫��ģ��, ���ַ�ʽ�͸�˹ģ������, �����ܸ��õı���ͼ��ı�Ե,
/// ��Chapter 13ϰ�����Ѿ�����ʹ��, �����������ȡ����������ܹ����õ�ȷ���߽�,
/// ��Ҳ�Ǳ�Ե����һ��˼·.
///***********************************************************************************

/// ����SSAO��Ҫ�ĳ�������������.
struct SSAOPassCB
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4 gOffsetVectors[14];

	// ģ��Ȩ��.
	float4 gBlurWeights[3];

	float2 gInvRenderTargetSize;

	// ��Щ��ֵ�����ڹ۲�ռ�����.
	float gOcclusionRadius;			// pΪԲ�İ���ķ�Χ.
	float gOcclusionFadeStart;		// ����SSAO�ڱεķ�Χ.
	float gOcclusionFadeEnd;
	float gSurfaceEpsilon;			// �������������С, �п���������ཻ.
};

/// SSAO���̳���������.
ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);

/// ���������������SSAO��������.
Texture2D gNormalMap : register(t0);
Texture2D gDepthMap  : register(t1);
Texture2D gInputMap  : register(t2);

/// ģ����ɺ������.
RWTexture2D<float4> gOutputMap : register(u0);

/// ���ܻ�ʹ�õ��ĳ��õľ�̬������.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamDepthMap		  : register(s7);

/// ģ���뾶.
static const int gBlurRadius = 5;

#define N 256
#define CacheSize (N + 2 * gBlurRadius)

// �����ڴ�, ������ɫ�����ߡ����.
groupshared float4 gColorCache[CacheSize];
groupshared float3 gNormalCache[CacheSize];
groupshared float gDepthCache[CacheSize];

/// NDC���ֵ-->�۲�ռ��������ֵ.
float NdcDepthToViewDepth(float z_ndc)
{
	// z_ndc = A + B / viewZ, ����A = gProj[2][2], B = gProj[3][2].
	float viewZ = gSSAOPassCB.gProj[3][2] / (z_ndc - gSSAOPassCB.gProj[2][2]);
	return viewZ;
}

/// ����ģ�������ļ�����ɫ��.
[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	// ģ������Ҫ��Ȩ��ֵ, ����ʵ����ֻ��ʹ��11��.
	float blurWeights[12] =
	{
		gSSAOPassCB.gBlurWeights[0].x, gSSAOPassCB.gBlurWeights[0].y,
		gSSAOPassCB.gBlurWeights[0].z, gSSAOPassCB.gBlurWeights[0].w,

		gSSAOPassCB.gBlurWeights[1].x, gSSAOPassCB.gBlurWeights[1].y,
		gSSAOPassCB.gBlurWeights[1].z, gSSAOPassCB.gBlurWeights[1].w,

		gSSAOPassCB.gBlurWeights[2].x, gSSAOPassCB.gBlurWeights[2].y,
		gSSAOPassCB.gBlurWeights[2].z, gSSAOPassCB.gBlurWeights[2].w
	};

	// �����ڴ�����������.
	if (groupThreadID.x < gBlurRadius)
	{
		int x = max(0, dispatchThreadID.x - gBlurRadius);

		gColorCache[groupThreadID.x] = gInputMap[int2(x, dispatchThreadID.y)];
		gNormalCache[groupThreadID.x] = gNormalMap[int2(x, dispatchThreadID.y)].rgb;
		gDepthCache[groupThreadID.x] = gDepthMap[int2(x, dispatchThreadID.y)].r;
	}

	// �����ڴ��ұ��������.
	if (groupThreadID.x >= N - gBlurRadius)
	{
		int x = min(gInputMap.Length.x - 1, dispatchThreadID.x + gBlurRadius);

		gColorCache[groupThreadID.x + 2 * gBlurRadius] = gInputMap[int2(x, dispatchThreadID.y)];
		gNormalCache[groupThreadID.x + 2 * gBlurRadius] = gNormalMap[int2(x, dispatchThreadID.y)].rgb;
		gDepthCache[groupThreadID.x + 2 * gBlurRadius] = gDepthMap[int2(x, dispatchThreadID.y)].r;
	}

	// �����������ڴ����.
	gColorCache[groupThreadID.x + gBlurRadius] = gInputMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)];
	gNormalCache[groupThreadID.x + gBlurRadius] = gNormalMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].rgb;
	gDepthCache[groupThreadID.x + gBlurRadius] = gDepthMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].r;

	GroupMemoryBarrierWithGroupSync();

	// �������زɼ�.
	float totalWeight = blurWeights[gBlurRadius];
	float4 color = totalWeight * gColorCache[groupThreadID.x + gBlurRadius];
	float3 centerNormal = gNormalCache[groupThreadID.x + gBlurRadius];
	float centerDepth = NdcDepthToViewDepth(gDepthCache[groupThreadID.x + gBlurRadius]);

	// �����ش���.	
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		if (i == 0)
			continue;

		// �ٽ����ز���.
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

/// ����ģ�������ļ�����ɫ��.
[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	// ģ������Ҫ��Ȩ��ֵ, ����ʵ����ֻ��ʹ��11��.
	float blurWeights[12] =
	{
		gSSAOPassCB.gBlurWeights[0].x, gSSAOPassCB.gBlurWeights[0].y,
		gSSAOPassCB.gBlurWeights[0].z, gSSAOPassCB.gBlurWeights[0].w,

		gSSAOPassCB.gBlurWeights[1].x, gSSAOPassCB.gBlurWeights[1].y,
		gSSAOPassCB.gBlurWeights[1].z, gSSAOPassCB.gBlurWeights[1].w,

		gSSAOPassCB.gBlurWeights[2].x, gSSAOPassCB.gBlurWeights[2].y,
		gSSAOPassCB.gBlurWeights[2].z, gSSAOPassCB.gBlurWeights[2].w
	};

	// �����ڴ��ϱ��������.
	if (groupThreadID.y < gBlurRadius)
	{
		int y = max(0, dispatchThreadID.y - gBlurRadius);

		gColorCache[groupThreadID.y] = gInputMap[int2(dispatchThreadID.x, y)];
		gNormalCache[groupThreadID.y] = gNormalMap[int2(dispatchThreadID.x, y)].rgb;
		gDepthCache[groupThreadID.y] = gDepthMap[int2(dispatchThreadID.x, y)].r;
	}

	// �����ڴ��±��������.
	if (groupThreadID.y >= N - gBlurRadius)
	{
		int y = min(gInputMap.Length.y - 1, dispatchThreadID.y + gBlurRadius);

		gColorCache[groupThreadID.y + 2 * gBlurRadius] = gInputMap[int2(dispatchThreadID.x, y)];
		gNormalCache[groupThreadID.y + 2 * gBlurRadius] = gNormalMap[int2(dispatchThreadID.x, y)].rgb;
		gDepthCache[groupThreadID.y + 2 * gBlurRadius] = gDepthMap[int2(dispatchThreadID.x, y)].r;
	}

	// �����������ڴ����.
	gColorCache[groupThreadID.y + gBlurRadius] = gInputMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)];
	gNormalCache[groupThreadID.y + gBlurRadius] = gNormalMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].rgb;
	gDepthCache[groupThreadID.y + gBlurRadius] = gDepthMap[min(dispatchThreadID.xy, gInputMap.Length.xy - 1)].r;

	GroupMemoryBarrierWithGroupSync();

	// �������زɼ�.
	float totalWeight = blurWeights[gBlurRadius];
	float4 color = totalWeight * gColorCache[groupThreadID.y + gBlurRadius];
	float3 centerNormal = gNormalCache[groupThreadID.y + gBlurRadius];
	float centerDepth = NdcDepthToViewDepth(gDepthCache[groupThreadID.y + gBlurRadius]);

	// �����ش���.
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		if (i == 0)
			continue;

		// �ٽ����ز���.
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