///***********************************************************************************
/// 光照计算工具类.
///***********************************************************************************

// 最多的光源数量/
#define MaxLights 16

/// 光源相关信息. (这里注意一下HLSL的数据填充)
struct Light
{
	float3 Strength;		// 光照强度.
	float FalloffStart;		// 点光源/聚光灯的起始衰减距离.
	float3 Direction;		// 点光源/聚光灯的光线方向.
	float FalloffEnd;		// 点光源/聚光灯的末尾衰减距离.(在这里之后就不会再收到这个光源影响了)
	float3 Position;		// 光源的位置信息.
	float SpotPower;		// 聚光灯的张角.
};

/// 计算光照所需要的材质信息.
struct Material
{
	float4 DiffuseAlbedo;		// 漫反射.
	float3 FresnelR0;			// 菲涅尔反射系数.
	float Shininess;			// 平滑度.
};

/// 线性计算关照衰减.
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// 菲涅尔反射的 Schlick 近似值.
// R0 = ( (n-1)/(n+1) )^2, 其中n是折射率.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
	float cosIncidentAngle = saturate(dot(normal, lightVec));

	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

	return reflectPercent;
}

/// BlinnPhong光照模型计算.
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
	const float m = mat.Shininess * 256.0f;

	// BlinnPhong光照模型计算的公式.
	float3 halfVec = normalize(toEye + lightVec);
	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	
	// 菲涅尔反射计算高光.
	float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);
	float3 specAlbedo = fresnelFactor * roughnessFactor;

	// 将specAlbedo限制范围在[0, 1], 并且保留平滑效果.
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

	// 漫反射 + 高光反射.
	return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

///---------------------------------------------------------------------------------------
/// 计算平行光光照.
///---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
	// 光向量获取..
	float3 lightVec = -L.Direction;

	// 兰伯特余弦定理获取光照强度.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

///---------------------------------------------------------------------------------------
/// 计算点光源的光照.
///---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// 计算光向量, 这里还没有归一化.
	float3 lightVec = L.Position - pos;

	// 点光源范围测试.
	float d = length(lightVec);
	if (d > L.FalloffEnd)
		return 0.0f;

	// 归一化光向量.
	lightVec /= d;

	// 兰伯特余弦定理获取光照强度.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	// 计算点光源的衰减..
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

///---------------------------------------------------------------------------------------
// 计算聚光灯的光照.
///---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// 和点光源类似, 聚光灯也需要位置信息和范围测试.
	float3 lightVec = L.Position - pos;

	float d = length(lightVec);
	if (d > L.FalloffEnd)
		return 0.0f;

	lightVec /= d;

	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// 聚光灯的聚光效果.
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

/// 通用的计算关照工具函数.
float4 ComputeLighting(Light gLights[MaxLights], Material mat,
	float3 pos, float3 normal, float3 toEye,
	float3 shadowFactor)
{
	float3 result = 0.0f;

	int i = 0;

#if (NUM_DIR_LIGHTS > 0)
	for (i = 0; i < NUM_DIR_LIGHTS; ++i)
	{
		result += shadowFactor[i] * ComputeDirectionalLight(gLights[i], mat, normal, toEye);
	}
#endif

#if (NUM_POINT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; ++i)
	{
		result += ComputePointLight(gLights[i], mat, pos, normal, toEye);
	}
#endif

#if (NUM_SPOT_LIGHTS > 0)
	for (i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; ++i)
	{
		result += ComputeSpotLight(gLights[i], mat, pos, normal, toEye);
	}
#endif 

	return float4(result, 0.0f);
}
