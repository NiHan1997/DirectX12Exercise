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
	float4 ShadowPos : POSITION;
	float2 TexC		 : TEXCOORD0;

	// 在这里, TtoWorld.xyz存储切线、法线、副切线的xyz分量, w分量存储世界坐标.
	float4 TtoWorld0 : TEXCOORD1;
	float4 TtoWorld1 : TEXCOORD2;
	float4 TtoWorld2 : TEXCOORD3;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;

	float3 posW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld).xyz;
	vout.PosH = mul(float4(posW, 1.0), gPassConstants.gViewProj);
	vout.ShadowPos = mul(float4(posW, 1.0), gPassConstants.gShadowTransform);

	MaterialData matData = gMaterialData[gObjectConstants.gMaterialIndex];
	float4 texC = mul(float4(vin.TexC, 0.0, 1.0), gObjectConstants.gTexTransform);
	vout.TexC = mul(texC, matData.MatTransform).xy;

	// 这里法线变换需要注意, 这里是在等比变换的基础上, 因此不需要使用逆转置矩阵.
	float3 normalW = normalize(mul(vin.NormalL, (float3x3)gObjectConstants.gWorld));
	float3 tangentW = normalize(mul(vin.TangentL.xyz, (float3x3)gObjectConstants.gWorld));
	float3 binormal = cross(normalW, tangentW) * vin.TangentL.w;

	vout.TtoWorld0 = float4(tangentW, posW.x);
	vout.TtoWorld1 = float4(binormal, posW.y);
	vout.TtoWorld2 = float4(normalW, posW.z);

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
	
	// 切线空间-->世界空间.
	float3 normalW = 2.0 * normalSample.rgb - 1.0;
	float3x3 tangentToWorld = float3x3(pin.TtoWorld0.xyz, pin.TtoWorld1.xyz, pin.TtoWorld2.xyz);
	normalW = normalize(mul(normalW, tangentToWorld));

	// 取出世界坐标.
	float3 posW = float3(pin.TtoWorld0.w, pin.TtoWorld1.w, pin.TtoWorld2.w);

#ifdef ALPHA_TEST
	clip(diffuseAlbedo.a - 0.1);
#endif	

	// 获取环境光.
	float4 ambient = gPassConstants.gAmbientLight * diffuseAlbedo;

	// 光照计算必需.
	float3 toEyeW = gPassConstants.gEyePosW - posW.xyz;
	float distToEye = length(toEyeW);
	toEyeW /= distToEye;

	// 光照计算.
	float3 shadowFactor = float3(1.0, 1.0, 1.0);
	shadowFactor[0] = CalcShadowFactor(pin.ShadowPos);

	const float gShininess = (1 - roughness) * normalSample.a;
	Material mat = { diffuseAlbedo, fresnelR0, gShininess };
	float4 dirLight = ComputeLighting(gPassConstants.gLights, mat, posW.xyz, normalW, toEyeW, shadowFactor);

	float4 finalColor = ambient + dirLight;

	// 添加立方体纹理采样.
	float3 reflectDir = reflect(-toEyeW, normalW);
	float4 reflectColor = gCubeMap.Sample(gsamAnisotropicWrap, reflectDir);

	// 计算菲涅尔反射程度.
	float3 fresnelFactor = SchlickFresnel(fresnelR0, normalW, reflectDir);
	finalColor.rgb += gShininess * fresnelFactor * reflectColor.rgb;

	finalColor.a = diffuseAlbedo.a;

	return finalColor;
}
