// ������Դ�Ķ���.
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

/// ObjectCB�ĳ����������ṹ��.
struct ObjectConstants
{
	float4x4 gWorld;
	float4x4 gTexTransform;

	uint gMaterialIndex;
	uint gObjPad0;
	uint gObjPad1;
	uint gObjPad2;
};

/// PassCB �ĳ����������ṹ��.
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

/// �������ݽṹ��.
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


/// ��������������.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);
ConstantBuffer<PassConstants> gPassConstants 	 : register(b1);

/// ��ɫ�����������Դ.
StructuredBuffer<MaterialData> gMaterialData : register(t0, space0);

/// ��պ�.
TextureCube gCubeMap       : register(t1, space0);

/// ���Դ��Ӱ.
TextureCube gShadowCubeMap : register(t2, space0);

/// ��ɫ������������Դ.
Texture2D gTexMap[6] : register(t3, space0);

/// ��̬������.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

/// ������Ӱ����.(NO PCF)
float CalcShadowFactor(float3 posW)
{
	// ������ռ��ж�����������������, ��ȡNDC���ֵ.
	float3 toLight = posW - gPassConstants.gEyePosW[1].xyz;

	// ����ǰ����任����Ӧ�������ͶӰ�ռ�, ��ȡNDC���ֵ.
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

	// ������û��ʹ��PCF�Ż�����, �Ѿ����Եõ������Ч��.
	float shadow = gShadowCubeMap.SampleCmpLevelZero(gsamShadow, toLight, currentDepth).r;

	return shadow;
}

/// ������Ӱ����.(PCF)
float CalcShadowFactorPCF(float3 posW)
{
	uint width, height, numMips;
	gShadowCubeMap.GetDimensions(0, width, height, numMips);

	// ������ƫ�������Ե���, �ڴ˷ֱ�����[2.0, 2.5]ƽ��Ч���Ϻ�.
	float dx = 2.3 / width;

	// ���������20������.
	float3 offsets[20] =
	{
		float3(dx, dx, dx), float3(dx, -dx, dx), float3(-dx, -dx, dx), float3(-dx, dx, dx),
		float3(dx, dx, -dx), float3(dx, -dx, -dx), float3(-dx, -dx, -dx), float3(-dx, dx, -dx),
		float3(dx, dx, 0), float3(dx, -dx, 0), float3(-dx, -dx, 0), float3(-dx, dx, 0),
		float3(dx, 0, dx), float3(-dx, 0, dx), float3(dx, 0, -dx), float3(-dx, 0, -dx),
		float3(0, dx, dx), float3(0, -dx, dx), float3(0, -dx, -dx), float3(0, dx, -dx)
	};

	// ������ռ��ж�����������������, ��ȡNDC���ֵ.
	float3 toLight = normalize(posW - gPassConstants.gEyePosW[1].xyz);

	// ����ǰ����任����Ӧ�������ͶӰ�ռ�, ��ȡNDC���ֵ.
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

	// PCF���ٽ������.
	float shadow = 0.0;
	[unroll]
	for (int j = 0; j < 20; ++j)
	{
		shadow += gShadowCubeMap.SampleCmpLevelZero(gsamShadow, toLight + offsets[j], currentPosH.z).r;
	}

	return shadow / 20.0;
}