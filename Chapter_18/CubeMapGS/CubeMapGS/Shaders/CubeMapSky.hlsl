///***********************************************************************************
/// 绘制反射天空的着色器.
///***********************************************************************************

#include "Common.hlsl"

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL : POSITION;
};

/// 几何着色器输入.
struct GS_CubeMap_IN
{
	float3 PosL : POSITION0;
	float4 PosW : POSITION1;
};

/// 几何着色器输出.
struct GS_CubeMap_OUT
{
	float4 PosH	   : SV_POSITION;
	float3 PosL    : POSITION;

	uint RTIndex : SV_RenderTargetArrayIndex;
};

/// 顶点着色器.
GS_CubeMap_IN VS (VertexIn vin)
{
	GS_CubeMap_IN vout;
	vout.PosL = vin.PosL;
	vout.PosW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld);

	return vout;
}

/// 几何着色器.
[maxvertexcount(18)]
void GS_CubeMap(triangle GS_CubeMap_IN input[3],
	inout TriangleStream<GS_CubeMap_OUT> CubeMapStream)
{
	for (int f = 0; f < 6; ++f)
	{
		GS_CubeMap_OUT output;

		output.RTIndex = f;

		for (int v = 0; v < 3; ++v)
		{
			output.PosL = input[v].PosL;
			output.PosH = mul(input[v].PosW, gPassConstants.gViewProj[1 + f]);

			CubeMapStream.Append(output);
		}
		CubeMapStream.RestartStrip();
	}
}

/// 像素着色器.
float4 PS(GS_CubeMap_OUT pin) : SV_Target
{
	return gCubeMap.Sample(gsamAnisotropicClamp, pin.PosL);
}
