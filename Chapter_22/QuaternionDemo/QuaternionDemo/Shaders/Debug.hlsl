///***********************************************************************************
/// 调试专用着色器, 在这里用于阴影调试.
///***********************************************************************************

#include "Common.hlsl"

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL : POSITION;
	float2 TexC : TEXCOORD0;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float2 TexC : TEXCOORD0;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;
	vout.PosH = float4(vin.PosL, 1.0);
	vout.TexC = vin.TexC;

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	return float4(gSsaoMap.Sample(gsamAnisotropicWrap, pin.TexC).rrr, 1.0);
}
