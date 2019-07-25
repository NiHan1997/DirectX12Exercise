///***********************************************************************************
/// 通用的不透明物体的着色器.
///***********************************************************************************

// 基础光源的定义.
#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 0
#endif

#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

/// ObjectCB的常量缓冲区结构体.
struct ObjectConstants
{
	float4x4 gWorld;
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
	Light gLights[MaxLights];
};


/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants 	 : register(b0);
ConstantBuffer<PassConstants> gPassConstants 		 : register(b1);

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL	   : POSITION;
	float4 Color   : COLOR;
};

/// 顶点着色器输出, 几何着色器输入.
struct VertexOut
{
	float3 PosW	   : POSITION;
	float4 Color   : COLOR;
};

/// 几何着色器输出, 像素着色器输入.
struct GeoOut
{
	float4 PosH  : SV_POSITION;
	float3 PosW  : POSITION;
	float4 Color : COLOR;
};

/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;

	vout.PosW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld).xyz;
	vout.Color = vin.Color;

	return vout;
}

/// 几何着色器.
[maxvertexcount(4)]
void GS(line VertexOut gin[2], inout LineStream<GeoOut> lineStream)
{
	// 输出顶点, 注意线条带的顺序.
	float4 v[4];
	v[0] = float4(gin[0].PosW.x, gin[0].PosW.y, gin[0].PosW.z, 1.0);
	v[1] = float4(gin[1].PosW.x, gin[1].PosW.y, gin[1].PosW.z, 1.0);
	v[2] = float4(gin[1].PosW.x, gin[1].PosW.y + 5.0, gin[1].PosW.z, 1.0);
	v[3] = float4(gin[0].PosW.x, gin[0].PosW.y + 5.0, gin[0].PosW.z, 1.0);

	GeoOut geoOut;
	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		geoOut.PosH = mul(v[i], gPassConstants.gViewProj);
		geoOut.PosW = v[i].xyz;
		geoOut.Color = gin[0].Color;

		lineStream.Append(geoOut);
	}
}

/// 像素着色器.
float4 PS (GeoOut pin) : SV_Target
{
	return pin.Color;
}
