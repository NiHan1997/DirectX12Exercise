///***********************************************************************************
///骷髅头渲染的着色器.
///***********************************************************************************

#include "Common.hlsl"

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL	    : POSITION;
	float3 NormalL  : NORMAL;
	float2 TexC     : TEXCOORD0;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float3 PosW    : POSITION;
	float3 NormalW : NORMAL;
	float2 TexC    : TEXCOORD0;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;

	vout.PosW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld).xyz;
	vout.PosH = mul(float4(vout.PosW, 1.0), gPassConstants.gViewProj);

	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 texC = mul(float4(vin.TexC, 0.0, 1.0), gObjectConstants.gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	// 这里法线变换需要注意, 这里是在等比变换的基础上, 因此不需要使用逆转置矩阵.
	vout.NormalW = normalize(mul(vin.NormalL, (float3x3)gObjectConstants.gWorld));

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 diffuseAlbedo = matData.DiffuseAlbedo;
	float3 fresnelR0 = matData.FresnelR0;
	float roughness = matData.Roughness;
	uint diffuseTexIndex = matData.DiffuseMapIndex;
	uint normalMapIndex = matData.NormalMapIndex;

	// 纹理采样.
	diffuseAlbedo *= gTexMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1);
#endif	

	// 获取环境光.
	float4 ambient = gPassConstants.gAmbientLight * diffuseAlbedo;

	// 光照计算必需.
	float3 toEyeW = gPassConstants.gEyePosW - pin.PosW.xyz;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;

	// 光照计算.
	const float gShininess = 1 - roughness;
	Material mat = { diffuseAlbedo, fresnelR0, gShininess };
	float4 dirLight = ComputeLighting(gPassConstants.gLights, mat, pin.PosW.xyz, pin.NormalW, toEyeW, 1.0);

	float4 finalColor = ambient + dirLight;

	// 添加立方体纹理采样.
	float3 reflectDir = reflect(-toEyeW, pin.NormalW);
	float4 reflectColor = gCubeMap.Sample(gsamAnisotropicWrap, reflectDir);

	// 计算菲涅尔反射程度.
	float3 fresnelFactor = SchlickFresnel(fresnelR0, pin.NormalW, reflectDir);
	finalColor.rgb += gShininess * fresnelFactor * reflectColor.rgb;

	finalColor.a = diffuseAlbedo.a;

	return finalColor;
}
