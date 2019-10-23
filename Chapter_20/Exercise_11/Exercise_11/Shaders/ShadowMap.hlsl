///***********************************************************************************
/// 负责深度写入的着色器, 用于阴影贴图的生成.
///***********************************************************************************

#include "Common.hlsl"

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL : POSITION;
	float2 TexC : TEXCOORD0;
};

/// 顶点着色器输出, 几何着色器输入.
struct GS_IN
{
	float4 PosW : SV_POSITION;
	float2 TexC : TEXCOORD0;
};

/// 几何着色器输出, 像素着色器输入.
struct GS_OUT
{
	float4 PosH  : SV_POSITION;
	float2 TexC  : TEXCOORD0;

	uint RTIndex : SV_RenderTargetArrayIndex;
};

/// 顶点着色器.
GS_IN VS(VertexIn vin)
{
	GS_IN vout;

	vout.PosW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld);

	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 texC = mul(float4(vin.TexC, 0.0, 1.0), gObjectConstants.gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	return vout;
}

/// 几何着色器绘制动态立方体纹理.
[maxvertexcount(18)]
void GS(triangle GS_IN input[3], inout TriangleStream<GS_OUT> CubeMapStream)
{
	for (int f = 0; f < 6; ++f)
	{
		GS_OUT output;

		output.RTIndex = f;

		for (int v = 0; v < 3; ++v)
		{
			output.PosH = mul(input[v].PosW, gPassConstants.gViewProj[1 + f]);
			output.TexC = input[v].TexC;

			CubeMapStream.Append(output);
		}
		CubeMapStream.RestartStrip();
	}
}


/// 像素着色器.
void PS(GS_OUT pin)
{
	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	uint diffuseTexIndex = matData.DiffuseMapIndex;

	// 纹理采样.
	diffuseAlbedo *= gTexMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1);
#endif	
}
