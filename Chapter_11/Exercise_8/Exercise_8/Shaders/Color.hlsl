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

/// ObjectCB的常量缓冲区结构体.
struct ObjectConstants
{
	float4x4 gWorld;
	float4x4 gTexTransform;
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
	float4 gFogColor;
	float gFogStart;
	float gFogRange;
	float2 cbPad1;
	Light gLights[MaxLights];
};

/// 模板参考值.
struct StencilConstants
{
	uint gStencil;
};

/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants 		 : register(b0);
ConstantBuffer<MaterialConstants> gMaterialConstants	 : register(b1);
ConstantBuffer<PassConstants> gPassConstants 			 : register(b2);
ConstantBuffer<StencilConstants> gStencilConstants 		 : register(b3);

/// 着色器输入纹理资源.
Texture2D gDiffuseMap : register(t0);

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
	float2 TexC    : TEXCOORD0;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH	   : SV_POSITION;
	float2 TexC    : TEXCOORD0;
};


/// 屏幕四个顶点的纹理坐标, 屏幕后处理特效的标配.
static const float2 gTexCoords[6] =
{
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

/// 顶点着色器.
VertexOut VS (uint vid : SV_VertexID)
{
	VertexOut vout;

	// 将顶点纹理坐标转化到NDC空间, 这里的效果就是覆盖整个窗口.
	vout.TexC = gTexCoords[vid];
	vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

/// 像素着色器.
float4 PS(VertexOut pin) : SV_Target
{
	float4 green = float4(0.48, 0.94, 0.41, 1.0);			// 绘制 1 次.
	float4 blue = float4(0.38, 0.84, 1.0, 1.0);				// 绘制 2 次.
	float4 orange = float4(0.96, 0.60, 0.05, 1.0);			// 绘制 3 次.
	float4 red = float4(0.96, 0.25, 0.38, 1.0);				// 绘制 4 次.
	float4 purple = float4(0.73, 0.53, 0.95, 1.0);			// 绘制 5 次.

	if (gStencilConstants.gStencil == 1)
	   return green;
	else if (gStencilConstants.gStencil == 2)
		return blue;
	else if (gStencilConstants.gStencil == 3)
		return orange;
	else if (gStencilConstants.gStencil == 4)
		return red;
	else
		return purple;
}
