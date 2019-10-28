///***********************************************************************************
/// ����SSAO�������ɫ��.
///***********************************************************************************

/// ����SSAO��Ҫ�ĳ�������������.
struct SSAOPassCB
{
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gProjTex;
	float4 gOffsetVectors[14];

	// SsaoBlur.hlslʹ�õ�����, ������ֻ�ǹ��ø�ǩ��.
	float4 gBlurWeights[3];

	float2 gInvRenderTargetSize;

	// ��Щ��ֵ�����ڹ۲�ռ�����.
	float gOcclusionRadius;			// pΪԲ�İ���ķ�Χ.
	float gOcclusionFadeStart;		// ����SSAO�ڱεķ�Χ.
	float gOcclusionFadeEnd;
	float gSurfaceEpsilon;			// �������������С, �п���������ཻ.
};

/// �����ﲻʹ��, ��ģ��SSAO�����ʱ��ʹ��.
struct BlurDir
{
	bool gHorizontalBlur;
};

ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);
ConstantBuffer<BlurDir> gBlurDirCB     : register(b1);

/// ��������������������������.
Texture2D gNormalMap       : register(t0);
Texture2D gDepthMap        : register(t1);
Texture2D gRandomVectorMap : register(t2);

/// ���ܻ�ʹ�õ��ĳ��õľ�̬������.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamDepthMap		  : register(s7);

// ����SSAO�Ĳ�������, ����Խ��Ч���϶�Խ��, ��������ҲԽ��.
static const int gSampleCount = 14;

/// ������Ļ����ʵ����Ҫ�Ķ���(����).
static const float2 gTexCoords[6] =
{
	float2(0.0, 1.0),
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 0.0),
	float2(1.0, 1.0)
};

/// ������ɫ������.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float3 PosV : POSITION;
	float2 TexC : TEXCOORD0;
};

/// ������ɫ��.
VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	vout.TexC = gTexCoords[vid];

	// ����Ļ������������ʾ��NDC�ռ�Ľ�ƽ����.
	// ��Ļ����ı��ʾ�����ԭʼ��̨�������Ļ����ϻ���һ���ı��γ�������Ļ��.
	vout.PosH = float4(2 * vout.TexC.x - 1.0, 1.0 - 2 * vout.TexC.y, 0.0, 1.0);

	// ��NDC�ռ�Ľ�ƽ��任���۲�ռ�Ľ�ƽ��.
	float4 posV = mul(vout.PosH, gSSAOPassCB.gInvProj);
	vout.PosV = posV.xyz / posV.w;

	return vout;
}

// ȷ��Ŀ���p��q���ڵ��̶�.
// distZ = p.z - r.z
// q��p����Χ�����һ��, r�ǹ۲�ռ��п����ڱ�p�Ŀ��ӵ�.
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
	/// ����˵��:
	/// p : ��Ҫ����SSAO��Ŀ���.
	/// n : ��pλ�õĹ۲�ռ䷨����.
	/// q : ���ƫ��q��һ��.
	/// r : �����ڱ�p�ĵ�.

	// ��ȡ��Ӧ���صĹ۲�ռ�ķ��ߺ����.
	float3 n = gNormalMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).xyz;
	float pz = gDepthMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).r;
	pz = NdcDepthToViewDepth(pz);

	/// �ع��۲�ռ��λ��.
	/// p = t * pin.PosV
	/// p.z = t * pin.PosV.z
	/// t = p.z / pin.PosV.z
	float3 p = (pz / pin.PosV.z) * pin.PosV;

	// ��ȡ�������, ������[0, 1], ������Ҫӳ�䵽[-1, 1].
	float3 randVec = 2.0 * gRandomVectorMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).rgb - 1.0;

	// ��pΪ����, ��p�ٽ���Χ����.
	float occlusionSum = 0.0;
	for (int i = 0; i < gSampleCount; ++i)
	{
		float3 offset = reflect(gSSAOPassCB.gOffsetVectors[i].xyz, randVec);

		// �ɼ���ȡ��q.
		float filp = sign(dot(offset, n));
		float3 q = p + filp * gSSAOPassCB.gOcclusionRadius * offset;

		// ��ȡ��q����������.
		float4 projQ = mul(float4(q, 1.0), gSSAOPassCB.gProjTex);
		projQ /= projQ.w;

		// ������ȡ�۲�㵽q������Ŀ��ӵ�r.
		float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0).r;
		rz = NdcDepthToViewDepth(rz);
		float3 r = (rz / q.z) * q;

		// ����r�Ƿ����ڱε�.
		float distZ = p.z - r.z;
		float dp = max(0, dot(n, normalize(r - p)));
		float occlusion = dp * OcclusionFunction(distZ);

		occlusionSum += occlusion;
	}

	occlusionSum /= gSampleCount;
	float access = 1.0 - occlusionSum;

	// �����Աȶ�.
	return saturate(pow(access, 8.0));
}