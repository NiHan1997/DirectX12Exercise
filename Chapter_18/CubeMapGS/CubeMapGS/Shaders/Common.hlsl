// 基础光源的定义.
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

/// MaterialCB的常量缓冲区结构体.
struct MaterialConstants
{
	float4 gDiffsueAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
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

	float4 gEyePosW[7];
	float4 gRenderTargetSize[7];
	float4 gInvRenderTargetSize[7];

	float4 gNearZ[7];
	float4 gFarZ[7];

	float gTotalTime;
	float gDeltaTime;
	float pad0;
	float pad1;

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
	uint MatPad0;
	uint MatPad1;
	uint MatPad2;
};


/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);
ConstantBuffer<PassConstants> gPassConstants 	 : register(b1);

/// 着色器输入材质资源.
StructuredBuffer<MaterialData> gMaterialData : register(t0, space0);

/// 立方体纹理资源.
TextureCube gCubeMap : register(t1, space0);

/// 着色器输入纹理资源.
Texture2D gDiffuseMap[3] : register(t2, space0);

/// 静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);