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


/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants 	 : register(b0);
ConstantBuffer<MaterialConstants> gMaterialConstants : register(b1);
ConstantBuffer<PassConstants> gPassConstants 		 : register(b2);

/// 着色器输入纹理资源.
Texture2D gDiffuseMap : register(t0);

/// 静态采样器.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

///
/// 顶点着色器阶段.
///

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL : POSITION;
};


/// 顶点着色器输出.
struct VertexOut
{
	float3 PosL : POSITION;
};


/// 顶点着色器, 在这里完全沦陷, 相当于充数但是有不可缺少的步骤.
VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosL = vin.PosL;

	return vout;
}



///
/// 外壳着色器阶段.
///

/// 曲面细分因子.
struct PatchTess
{
	float EdgeTess[3]   : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};


/// 常量外壳着色器.
PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	float3 centerL = 0.33 * (patch[0].PosL + patch[1].PosL + patch[2].PosL);
	float3 centerW = mul(float4(centerL, 1.0), gObjectConstants.gWorld).xyz;

	// 根据距离确定细分次数.
	float d = distance(centerW, gPassConstants.gEyePosW);

	const float d0 = 20.0;
	const float d1 = 100.0;
	float tess = 64.0 * saturate((d1 - d) / (d1 - d0));

	// 曲面细分因子获取.
	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;

	pt.InsideTess = tess;

	return pt;
}


/// 外壳着色器输出.
struct HollOut
{
	float3 PosL : POSITION;
};

/// 控制点外壳着色器.
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0)]
HollOut HS(InputPatch<VertexOut, 3> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
	HollOut hout;
	hout.PosL = p[i].PosL;

	return hout;
}



///
/// 域着色器阶段.
///

/// 域着色器输出.
struct DomainOut
{
	float4 PosH : SV_POSITION;
};

[domain("tri")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HollOut, 3> quad)
{
	DomainOut dout;

	// 双线性插值.
	float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
	float3 v2 = lerp(quad[0].PosL, quad[2].PosL, uv.y);
	float3 p = v1 + v2;

	// 计算高度.
	p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));


	float4 posW = mul(float4(p, 1.0), gObjectConstants.gWorld);
	dout.PosH = mul(posW, gPassConstants.gViewProj);

	return dout;
}


/// 像素着色器.
float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}