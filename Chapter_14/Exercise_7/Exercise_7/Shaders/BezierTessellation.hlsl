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
	float EdgeTess[4]   : SV_TessFactor;
	float InsideTess[2] : SV_InsideTessFactor;
};


/// 常量外壳着色器.
PatchTess ConstantHS(InputPatch<VertexOut, 9> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	// 曲面细分因子获取.
	pt.EdgeTess[0] = 25;
	pt.EdgeTess[1] = 25;
	pt.EdgeTess[2] = 25;
	pt.EdgeTess[3] = 25;

	pt.InsideTess[0] = 25;
	pt.InsideTess[1] = 25;

	return pt;
}



/// 外壳着色器输出.
struct HullOut
{
	float3 PosL : POSITION;
};

/// 控制点外壳着色器.
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(9)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0)]
HullOut HS(InputPatch<VertexOut, 9> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
{
	HullOut hout;
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

/// 计算二次贝塞尔曲线的伯恩斯坦基系数.
float3 BernsteinBasis(float t)
{
	float invT = 1.0 - t;

	return float3(invT * invT,
		2.0 * t * invT,
		t * t);
}

/// 求贝塞尔曲面上的点.
float3 CubicBezierSum(const OutputPatch<HullOut, 9> bezpatch, float3 basisU, float3 basisV)
{
	float3 sum = float3(0.0f, 0.0f, 0.0f);
	sum = basisV.x * (basisU.x * bezpatch[0].PosL + basisU.y * bezpatch[1].PosL + 
		basisU.z * bezpatch[2].PosL);

	sum += basisV.y * (basisU.x * bezpatch[3].PosL + basisU.y * bezpatch[4].PosL + 
		basisU.z * bezpatch[5].PosL);

	sum += basisV.z * (basisU.x * bezpatch[6].PosL + basisU.y * bezpatch[7].PosL + 
		basisU.z * bezpatch[8].PosL);

	return sum;
}

/// 域着色器.
[domain("quad")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HullOut, 9> bezPatch)
{
	DomainOut dout;

	float3 basisU = BernsteinBasis(uv.x).xyz;
	float3 basisV = BernsteinBasis(uv.y).xyz;

	float3 p = CubicBezierSum(bezPatch, basisU, basisV);

	float4 posW = mul(float4(p, 1.0f), gObjectConstants.gWorld);
	dout.PosH = mul(posW, gPassConstants.gViewProj);

	return dout;
}


/// 像素着色器.
float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}