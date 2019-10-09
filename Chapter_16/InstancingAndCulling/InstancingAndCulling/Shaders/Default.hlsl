///***********************************************************************************
/// 通用的不透明物体的着色器.
///***********************************************************************************

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

/// PassCB 的常量缓冲区结构体.
struct PassConstants
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;

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

/// 每一个实例物体数据结构体.
struct InstanceData
{
	float4x4 World;
	float4x4 TexTransform;

	uint MaterialIndex;
	uint ObjPad0;
	uint ObjPad1;
	uint ObjPad2;
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
ConstantBuffer<PassConstants> gPassConstants : register(b0);

/// 着色器输入材质资源.
StructuredBuffer<InstanceData> gInstanceData : register(t0, space0);
StructuredBuffer<MaterialData> gMaterialData : register(t1, space0);

/// 着色器输入纹理资源.
Texture2D gDiffuseMap[7] : register(t2, space0);

/// 静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL	   : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD0;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH	   : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD0;

	nointerpolation uint MatIndex : MATINDEX;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout;

	vout.PosW = mul(float4(vin.PosL, 1.0), gInstanceData[instanceID].World).xyz;
	vout.PosH = mul(float4(vout.PosW, 1.0), gPassConstants.gViewProj);

	// 这里法线变换需要注意, 这里是在等比变换的基础上, 因此不需要使用逆转置矩阵.
	vout.NormalW = mul(vin.NormalL, (float3x3)gInstanceData[instanceID].World);

	vout.MatIndex = gInstanceData[instanceID].MaterialIndex;

	float4 texC = mul(float4(vin.TexC, 0.0, 1.0), gInstanceData[instanceID].TexTransform);
	vout.TexC = mul(texC, gMaterialData[vout.MatIndex].MatTransform).xy;

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	MaterialData matData = gMaterialData[pin.MatIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;

	// 纹理采样.
	diffuseAlbedo *= gDiffuseMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1);
#endif	

	// 获取环境光.
	float4 ambient = gPassConstants.gAmbientLight * diffuseAlbedo;

	// 光照计算必需.
	pin.NormalW = normalize(pin.NormalW);
	float3 toEyeW = gPassConstants.gEyePosW - pin.PosW;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;

	// 光照计算.
	const float gShininess = 1 - roughness;
	Material mat = { diffuseAlbedo, fresnelR0, gShininess };
	float4 dirLight = ComputeLighting(gPassConstants.gLights, mat, pin.PosW.xyz, pin.NormalW, toEyeW, 1.0);

	float4 finalColor = ambient + dirLight;

#ifdef FOG
	float fogAmount = saturate((distToEye - gPassConstants.gFogStart) / gPassConstants.gFogRange);
	finalColor = lerp(finalColor, gPassConstants.gFogColor, fogAmount);
#endif

	finalColor.a = diffuseAlbedo.a;

	return finalColor;
}
