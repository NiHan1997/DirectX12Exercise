#include "LightingUtil.hlsl"

/// ObjectCB�ĳ����������ṹ��.
struct ObjectConstants
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};

/// MaterialCB�ĳ����������ṹ��.
struct MaterialConstants
{
	float4 gDiffsueAlbedo;
	float3 gFresnelR0;
	float gRoughness;
	float4x4 gMatTransform;
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


/// ��������������.
ConstantBuffer<ObjectConstants> gObjectConstants 	 : register(b0);
ConstantBuffer<MaterialConstants> gMaterialConstants : register(b1);
ConstantBuffer<PassConstants> gPassConstants 		 : register(b2);

/// ��ɫ������������Դ.
Texture2D gDiffuseMap : register(t0);

/// ��̬������.
SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

///
/// ������ɫ���׶�.
///

/// ������ɫ������.
struct VertexIn
{
	float3 PosL : POSITION;
};


/// ������ɫ�����.
struct VertexOut
{
	float3 PosL : POSITION;
};


/// ������ɫ��, ��������ȫ����, �൱�ڳ��������в���ȱ�ٵĲ���.
VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosL = vin.PosL;

	return vout;
}



///
/// �����ɫ���׶�.
///

/// ����ϸ������.
struct PatchTess
{
	float EdgeTess[3]   : SV_TessFactor;
	float InsideTess : SV_InsideTessFactor;
};


/// ���������ɫ��.
PatchTess ConstantHS(InputPatch<VertexOut, 3> patch, uint patchID : SV_PrimitiveID)
{
	PatchTess pt;

	float3 centerL = 0.33 * (patch[0].PosL + patch[1].PosL + patch[2].PosL);
	float3 centerW = mul(float4(centerL, 1.0), gObjectConstants.gWorld).xyz;

	// ���ݾ���ȷ��ϸ�ִ���.
	float d = distance(centerW, gPassConstants.gEyePosW);

	const float d0 = 20.0;
	const float d1 = 100.0;
	float tess = 64.0 * saturate((d1 - d) / (d1 - d0));

	// ����ϸ�����ӻ�ȡ.
	pt.EdgeTess[0] = tess;
	pt.EdgeTess[1] = tess;
	pt.EdgeTess[2] = tess;

	pt.InsideTess = tess;

	return pt;
}


/// �����ɫ�����.
struct HollOut
{
	float3 PosL : POSITION;
};

/// ���Ƶ������ɫ��.
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
/// ����ɫ���׶�.
///

/// ����ɫ�����.
struct DomainOut
{
	float4 PosH : SV_POSITION;
};

[domain("tri")]
DomainOut DS(PatchTess patchTess, float2 uv : SV_DomainLocation, const OutputPatch<HollOut, 3> quad)
{
	DomainOut dout;

	// ˫���Բ�ֵ.
	float3 v1 = lerp(quad[0].PosL, quad[1].PosL, uv.x);
	float3 v2 = lerp(quad[0].PosL, quad[2].PosL, uv.y);
	float3 p = v1 + v2;

	// ����߶�.
	p.y = 0.3f * (p.z * sin(p.x) + p.x * cos(p.z));


	float4 posW = mul(float4(p, 1.0), gObjectConstants.gWorld);
	dout.PosH = mul(posW, gPassConstants.gViewProj);

	return dout;
}


/// ������ɫ��.
float4 PS(DomainOut pin) : SV_Target
{
    return float4(1.0f, 1.0f, 1.0f, 1.0f);
}