///***********************************************************************************
/// 绘制法线纹理的着色器.
///***********************************************************************************

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL	   : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
	float4 PosH		: SV_POSITION;
    float3 NormalW  : NORMAL;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	float4 posW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld);
	vout.PosH = mul(posW, gPassConstants.gViewProj);

	vout.NormalW = mul(vin.NormalL, (float3x3)gObjectConstants.gWorld);
	
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 normalV = mul(pin.NormalW, (float3x3)gPassConstants.gView);

    return float4(normalV, 0.0f);
}
