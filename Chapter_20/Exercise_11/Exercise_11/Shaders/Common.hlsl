// 基础光源的定义.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

/// ObjectCB的常量缓冲区结构体.
struct ObjectConstants
{
	float4x4 gWorld;
	float4x4 gTexTransform;

	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

/// PassCB 的常量缓冲区结构体.
struct PassConstants
{
	float4x4 gView[7];
	float4x4 gInvView[7];
	float4x4 gProj[7];
	float4x4 gInvProj[7];
	float4x4 gViewProj[7];
	float4x4 gInvViewProj[7];

	float3 gEyePosW[7];

	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;

	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;

	float4 gAmbientLight;
	Light gLights[MaxLights];
};

/// 材质数据结构体.
struct MaterialData
{
	float4 DiffuseAlbedo;
	float3 FresnelR0;
	float Roughness;
	float4x4 MatTransform;
	uint DiffuseMapIndex;
	uint NormalMapIndex;
	uint MatPad0;
	uint MatPad1;	
};


/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);
ConstantBuffer<PassConstants> gPassConstants 	 : register(b1);

/// 着色器输入材质资源.
StructuredBuffer<MaterialData> gMaterialData : register(t0, space0);

/// 天空盒.
TextureCube gCubeMap       : register(t1, space0);

/// 点光源阴影.
TextureCube gShadowCubeMap : register(t2, space0);

/// 着色器输入纹理资源.
Texture2D gTexMap[6] : register(t3, space0);

/// 静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

/// 计算阴影因子.(NO PCF)
float CalcShadowFactor(float3 posW)
{
	// 在世界空间中对深度立方体纹理采样, 获取NDC深度值.
	float3 toLight = posW - gPassConstants.gEyePosW[1].xyz;

	// 将当前坐标变换到对应摄像机的投影空间, 获取NDC深度值.
	float4 temp;
	[unroll]
	for (int i = 1; i < 7; ++i)
	{
		temp = mul(float4(posW, 1.0), gPassConstants.gViewProj[i]);
		temp.xyz /= temp.w;

		if (temp.x >= -1 && temp.x <= 1 &&
			temp.y >= -1 && temp.y <= 1 &&
			temp.z >= 0 && temp.z <= 1)
			break;
	}

	float currentDepth = temp.z;

	// 在这里没有使用PCF优化方法, 已经可以得到不错的效果.
	float shadow = gShadowCubeMap.SampleCmpLevelZero(gsamShadow, toLight, currentDepth).r;

	return shadow;
}

/// 计算阴影因子.(PCF)
float CalcShadowFactorPCF(float3 posW)
{
	uint width, height, numMips;
	gShadowCubeMap.GetDimensions(0, width, height, numMips);

	// 在这里偏移量测试得来, 在此分辨率下[2.0, 2.5]平滑效果较好.
	float dx = 2.3 / width;

	// 大致随机的20个向量.
	float3 offsets[20] =
	{
		float3(dx, dx, dx), float3(dx, -dx, dx), float3(-dx, -dx, dx), float3(-dx, dx, dx),
		float3(dx, dx, -dx), float3(dx, -dx, -dx), float3(-dx, -dx, -dx), float3(-dx, dx, -dx),
		float3(dx, dx, 0), float3(dx, -dx, 0), float3(-dx, -dx, 0), float3(-dx, dx, 0),
		float3(dx, 0, dx), float3(-dx, 0, dx), float3(dx, 0, -dx), float3(-dx, 0, -dx),
		float3(0, dx, dx), float3(0, -dx, dx), float3(0, -dx, -dx), float3(0, dx, -dx)
	};

	// 在世界空间中对深度立方体纹理采样, 获取NDC深度值.
	float3 toLight = normalize(posW - gPassConstants.gEyePosW[1].xyz);

	// 将当前坐标变换到对应摄像机的投影空间, 获取NDC深度值.
	float4 currentPosH;
	[unroll]
	for (int i = 1; i < 7; ++i)
	{
		currentPosH = mul(float4(posW, 1.0), gPassConstants.gViewProj[i]);
		currentPosH.xyz /= currentPosH.w;

		if (currentPosH.x >= -1 && currentPosH.x <= 1 &&
			currentPosH.y >= -1 && currentPosH.y <= 1 &&
			currentPosH.z >= 0 && currentPosH.z <= 1)
			break;
	}

	// PCF对临近点采样.
	float shadow = 0.0;
	[unroll]
	for (int j = 0; j < 20; ++j)
	{
		shadow += gShadowCubeMap.SampleCmpLevelZero(gsamShadow, toLight + offsets[j], currentPosH.z).r;
	}

	return shadow / 20.0;
}