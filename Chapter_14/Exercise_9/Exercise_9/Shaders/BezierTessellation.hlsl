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
	float EdgeTess[3] : SV_TessFactor;
	float InsideTess  : SV_InsideTessFactor;
};


/// 常量外壳着色器.
PatchTess ConstantHS(InputPatch<VertexOut, 10> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	// 曲面细分因子获取.
	pt.EdgeTess[0] = 25;
	pt.EdgeTess[1] = 25;
	pt.EdgeTess[2] = 25;

	pt.InsideTess = 25;

	return pt;
}



/// 外壳着色器输出.
struct HullOut
{
	float3 PosL : POSITION;
};

/// 控制点外壳着色器.
[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(10)]
[patchconstantfunc("ConstantHS")]
[maxtessfactor(64.0)]
HullOut HS(InputPatch<VertexOut, 10> p, uint i : SV_OutputControlPointID, uint patchID : SV_PrimitiveID)
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

/// 求三角形贝塞尔曲面上的点.
float3 CubicBezierSum(const OutputPatch<HullOut, 10> bezpatch, 
	float basisU, float basisV, float basisW)
{
	float3 sum = float3(0.0f, 0.0f, 0.0f);

	// 计算可以参考PDF资料.
	sum = pow(basisV, 3) * bezpatch[0].PosL + 
		3 * basisU * pow(basisV, 2) * bezpatch[1].PosL + 
		3 * pow(basisV, 2) * basisW * bezpatch[2].PosL;

	sum += 3 * pow(basisU, 2) * basisV * bezpatch[3].PosL +
		6 * basisU * basisV * basisW * bezpatch[4].PosL +
		3 * pow(basisV, 2) * basisW * bezpatch[5].PosL;

	sum += pow(basisU, 3) * bezpatch[6].PosL +
		3 * pow(basisU, 2) * basisW * bezpatch[7].PosL +
		3 * basisU * pow(basisW, 2) * bezpatch[8].PosL +
		pow(basisW, 3) * bezpatch[9].PosL;

	return sum;
}

/// 域着色器.
[domain("tri")]
DomainOut DS(PatchTess patchTess, float3 uvw : SV_DomainLocation, const OutputPatch<HullOut, 10> bezPatch)
{
	DomainOut dout;

	float3 p = CubicBezierSum(bezPatch, uvw.x, uvw.y, uvw.z);

	float4 posW = mul(float4(p, 1.0f), gObjectConstants.gWorld);
	dout.PosH = mul(posW, gPassConstants.gViewProj);

	return dout;
}


/// 像素着色器.
float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}