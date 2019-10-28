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

/// 模糊的方向. T : 横向模糊, F : 纵向模糊.
struct BlurDir
{
	bool gHorizontalBlur;
};

ConstantBuffer<SSAOPassCB> gSSAOPassCB : register(b0);
ConstantBuffer<BlurDir> gBlurDirCB     : register(b1);

/// 法线纹理、深度纹理、随机向量纹理.
Texture2D gNormalMap : register(t0);
Texture2D gDepthMap  : register(t1);
Texture2D gInputMap  : register(t2);

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

static const float2 gTexCoords[6] =
{
	float2(0.0, 1.0),
	float2(0.0, 0.0),
	float2(1.0, 0.0),
	float2(0.0, 1.0),
	float2(1.0, 0.0),
	float2(1.0, 1.0)
};

/// 顶点着色器输出.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD;
};

/// 顶点着色器.
VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.0 * vout.TexC.x - 1.0, 1.0 - 2.0 * vout.TexC.y, 0.0, 1.0);

	return vout;
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

	// 确认模糊的方向.
	float2 texOffset;
	if (gBlurDirCB.gHorizontalBlur)
	{
		texOffset = float2(gSSAOPassCB.gInvRenderTargetSize.x, 0.0);
	}
	else
	{
		texOffset = float2(0.0, gSSAOPassCB.gInvRenderTargetSize.y);
	}

	// 中心点首先采样, 其他像素需要和中心点进行对比.
	float4 color = blurWeights[gBlurRadius] * gInputMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0);
	float totalWeight = blurWeights[gBlurRadius];

	float3 centerNormal = gNormalMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).rgb;
	float centerDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearWrap, pin.TexC, 0.0).r);

	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		// 中心点已经采样.
		if (i == 0)
			continue;

		// 偏移像素采样.
		float2 tex = pin.TexC + i * texOffset;

		float3 neighborNormal = gNormalMap.SampleLevel(gsamLinearWrap, tex, 0.0).rgb;
		float neighborDepth = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamLinearWrap, tex, 0.0).r);

		// 对比两个像素差异, 确定边界.
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