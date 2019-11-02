// ������Դ�Ķ���.
#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
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
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float4x4 gViewProjTex;
	float4x4 gShadowTransform;

	float3 gEyePosW;
	float cbPad0;
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

/// ������������Դ.
TextureCube gCubeMap : register(t1, space0);
Texture2D gShadowMap : register(t2, space0);
Texture2D gSsaoMap   : register(t3, space0);

/// ��ɫ������������Դ.
Texture2D gTexMap[6] : register(t4, space0);

/// ��̬������.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);
SamplerState gsamDepthMap		  : register(s7);

/// ������Ӱ����.
float CalcShadowFactor(float4 shadowPosH)
{
	// �任��NDC�ռ����, �������ֵ�Ƚ�.
	shadowPosH.xyz /= shadowPosH.w;

	// NDC���ֵ.
	float depth = shadowPosH.z;

	uint width, height, numMips;
	gShadowMap.GetDimensions(0, width, height, numMips);

	// ��Χƫ�Ʋ���.
	float dx = 1.0 / width;	
	const float2 offsets[9] =
	{
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	float percentLit = 0.0;
	[unroll]
	for (int i = 0; i < 9; ++i)
	{
		percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
	}

	return percentLit / 9.0;
}