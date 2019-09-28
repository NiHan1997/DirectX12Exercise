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
	float gRadius;
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

///
/// 顶点着色器阶段.
///

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL	   : POSITION;
	float4 Color   : COLOR;
};

/// 顶点着色器输出, 几何着色器输入.
struct VertexOut
{
	float3 PosL	   : POSITION;
	float4 Color   : COLOR;
};

/// 顶点着色器.
VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	vout.PosL = vin.PosL;
	vout.Color = vin.Color;

	return vout;
}



///
/// 外壳着色器阶段.
///

/// 曲面细分因子.
struct PatchTess
{
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess  : SV_InsideTessFactor;
};

/// 常量外壳着色器.
PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	float3 centerL = float3(0.0, 0.0, 0.0);
	float3 centerW = mul(float4(centerL, 1.0), gObjectConstants.gWorld).xyz;

	// 根据距离确定细分次数.
	float d = distance(centerW, gPassConstants.gEyePosW);

	const float d0 = 20.0;
	const float d1 = 100.0;
	float tess = 5.0 * saturate((d1 - d) / (d1 - d0));

	// 曲面细分因子获取.
	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;

	pt.InsideTess = tess;

	return pt;
}

/// 外壳着色器输出.
struct HullOut
{
	float3 PosL  : POSITION;
	float4 Color : COLOR; 
};

/// 控制点外壳着色器.
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(5.0)]
HullOut HS(InputPatch<VertexOut, 3> p, uint i : SV_OutputControlPointID)
{
	HullOut hout;
	hout.PosL = p[i].PosL;
	hout.Color = p[i].Color;

	return hout;
}

///
/// 域着色器阶段.
///

/// 域着色器输出.
struct DomainOut
{
	float4 PosH  : SV_POSITION;
	float4 Color : COLOR;
};

[domain("tri")]
DomainOut DS(PatchTess patchTess, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 3> tri)
{
	DomainOut dout;

	// 这里是三角形坐标对应的难点, 可以参考书上附录C3, 这里是重心坐标.
	float3 v1 = tri[0].PosL * uvw.x;
	float3 v2 = tri[1].PosL * uvw.y;
	float3 v3 = tri[2].PosL * uvw.z;
	float3 p = normalize(v1 + v2 + v3) * gObjectConstants.gRadius;

	float4 posW = mul(float4(p, 1.0), gObjectConstants.gWorld);
	dout.PosH = mul(posW, gPassConstants.gViewProj);
	dout.Color = tri[0].Color;

	return dout;
}

/// 像素着色器.
float4 PS(DomainOut pin) : SV_Target
{
	return pin.Color;
}
