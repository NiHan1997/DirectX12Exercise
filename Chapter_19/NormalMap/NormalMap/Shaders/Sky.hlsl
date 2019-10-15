///***********************************************************************************
/// 天空盒绘制着色器.
///***********************************************************************************

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH : SV_POSITION;
    float3 PosL : POSITION;
};
 
VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosL = vin.PosL;
	float4 posW = mul(float4(vin.PosL, 1.0f), gObjectConstants.gWorld);

	// 保证摄像机在天空盒的中心.
	posW.xyz += gPassConstants.gEyePosW;

	// 保证天空盒在远平面处.
	vout.PosH = mul(posW, gPassConstants.gViewProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	return gCubeMap.Sample(gsamAnisotropicClamp, pin.PosL);
}

