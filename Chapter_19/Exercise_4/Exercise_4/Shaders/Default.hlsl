///***********************************************************************************
/// 通用的不透明物体的着色器.
///***********************************************************************************

#include "Common.hlsl"

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL	    : POSITION;
	float3 NormalL  : NORMAL;
	float4 TangentL : TANGENT;
	float2 TexC     : TEXCOORD0;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH		 : SV_POSITION;
	float3 PosT		 : POSITION;
	float2 TexC		 : TEXCOORD0;
	float3 ViewDirT  : TEXCOORD1;
	float3 LightDirT[3] : TEXCOORD2;
	float3 LightPosT[3] : TEXCOORD5;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;

	float3 posW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld).xyz;
	vout.PosH = mul(float4(posW, 1.0), gPassConstants.gViewProj);

	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 texC = mul(float4(vin.TexC, 0.0, 1.0), gObjectConstants.gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	// 这里法线变换需要注意, 这里是在等比变换的基础上, 因此不需要使用逆转置矩阵.
	float3 normalW = normalize(mul(vin.NormalL, (float3x3)gObjectConstants.gWorld));
	float3 tangentW = normalize(mul(vin.TangentL.xyz, (float3x3)gObjectConstants.gWorld));
	float3 binormal = cross(normalW, tangentW) * vin.TangentL.w;

	float3x3 worldToTangent = float3x3(
		tangentW.x, binormal.x, normalW.x,
		tangentW.y, binormal.y, normalW.y,
		tangentW.z, binormal.z, normalW.z);

	vout.PosT = mul(posW, worldToTangent);
	vout.ViewDirT = mul(gPassConstants.gEyePosW - posW, worldToTangent);

	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		vout.LightDirT[i] = normalize(mul(gPassConstants.gLights[i].Direction, worldToTangent));
		vout.LightPosT[i] = normalize(mul(gPassConstants.gLights[i].Position, worldToTangent));
	}

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
	float4 normalSample = gTexMap[normalMapIndex].Sample(gsamAnisotropicWrap, pin.TexC);
	
	// 获取数据.
	float3 normalT = 2.0 * normalSample.rgb - 1.0;
	float3 viewDirT = normalize(pin.ViewDirT);

	// 光源数据.
	Light lights[3];
	for (int i = 0; i < 3; ++i)
	{
		lights[i].Strength = gPassConstants.gLights[i].Strength;
		lights[i].Direction = pin.LightDirT[i];
		lights[i].Position = pin.LightPosT[i];
	}

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1);
#endif	

	// 获取环境光.
	float4 ambient = gPassConstants.gAmbientLight * diffuseAlbedo;

	// 光照计算.
	const float gShininess = (1 - roughness) * normalSample.a;
	Material mat = { diffuseAlbedo, fresnelR0, gShininess };
	float4 dirLight = ComputeLighting(lights, mat, pin.PosT, normalT, viewDirT, 1.0);

	float4 finalColor = ambient + dirLight;
	finalColor.a = diffuseAlbedo.a;

	return finalColor;
}
