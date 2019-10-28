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

/// ģ���ķ���. T : ����ģ��, F : ����ģ��.
struct BlurDir
{
	bool gHorizontalBlur;
};

ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);
ConstantBuffer<BlurDir> gBlurDirCB     : register(b1);

/// ��������������������������.
Texture2D gNormalMap : register(t0);
Texture2D gDepthMap  : register(t1);
Texture2D gInputMap  : register(t2);

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

static const float2 gTexCoords[6] =
{
	float2(0.0, 1.0),
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 0.0),
	float2(1.0, 1.0)
};

/// ������ɫ�����.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD;
};

/// ������ɫ��.
VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.0 * vout.TexC.x - 1.0, 1.0 - 2.0 * vout.TexC.y, 0.0, 1.0);

	return vout;
}

/// NDC���ֵ-->�۲�ռ��������ֵ.
float NdcDepthToViewDepth(float z_ndc)
{
	// z_ndc = A + B / viewZ, ����A = gProj[2][2], B = gProj[3][2].
	float viewZ = gSSAOPassCB.gProj[3][2] / (z_ndc - gSSAOPassCB.gProj[2][2]);
	return viewZ;
}

/// ������ɫ��.
float4 PS(VertexOut pin) : SV_Target
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

	// ȷ��ģ���ķ���.
	float2 texOffset;
	if (gBlurDirCB.gHorizontalBlur)
	{
		texOffset = float2(gSSAOPassCB.gInvRenderTargetSize.x, 0.0);
	}
	else
	{
		texOffset = float2(0.0, gSSAOPassCB.gInvRenderTargetSize.y);
	}

	// ���ĵ����Ȳ���, ����������Ҫ�����ĵ���жԱ�.
	float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0);
	float totalWeight = blurWeights[gBlurRadius];

	float3 centerNormal = gNormalMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).rgb;
	float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).r);

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		// ���ĵ��Ѿ�����.
		if (i == 0)
			continue;

		// ƫ�����ز���.
		float2 tex = pin.TexC + i * texOffset;

		float3 neighborNormal = gNormalMap.SampleLevel(gsamLinearWrap, tex, 0.0).rgb;
		float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearWrap, tex, 0.0).r);

		// �Ա��������ز���, ȷ���߽�.
		if (dot(neighborNormal, centerNormal) >= 0.8 && 
			abs(neighborDepth - centerDepth) <= 0.2)
		{
			float weight = blurWeights[i + gBlurRadius];

			color += weight * gInputMap.SampleLevel(gsamLinearWrap, tex, 0.0);
			totalWeight += weight;
		}
	}

	return color / totalWeight;
}