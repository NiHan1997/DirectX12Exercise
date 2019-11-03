///***********************************************************************************
/// ���ռ��㹤����.
///***********************************************************************************

// ���Ĺ�Դ����/
#define MaxLights 16

/// ��Դ�����Ϣ. (����ע��һ��HLSL���������)
struct Light
{
	float3 Strength;		// ����ǿ��.
	float FalloffStart;		// ���Դ/�۹�Ƶ���ʼ˥������.
	float3 Direction;		// ���Դ/�۹�ƵĹ��߷���.
	float FalloffEnd;		// ���Դ/�۹�Ƶ�ĩβ˥������.(������֮��Ͳ������յ������ԴӰ����)
	float3 Position;		// ��Դ��λ����Ϣ.
	float SpotPower;		// �۹�Ƶ��Ž�.
};

/// �����������Ҫ�Ĳ�����Ϣ.
struct Material
{
	float4 DiffuseAlbedo;		// ������.
	float3 FresnelR0;			// ����������ϵ��.
	float Shininess;			// ƽ����.
};

/// ���Լ������˥��.
float CalcAttenuation(float d, float falloffStart, float falloffEnd)
{
	return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// ����������� Schlick ����ֵ.
// R0 = ( (n-1)/(n+1) )^2, ����n��������.
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec)
{
	float cosIncidentAngle = saturate(dot(normal, lightVec));

	float f0 = 1.0f - cosIncidentAngle;
	float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

	return reflectPercent;
}

/// BlinnPhong����ģ�ͼ���.
float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
	const float m = mat.Shininess * 256.0f;

	// BlinnPhong����ģ�ͼ���Ĺ�ʽ.
	float3 halfVec = normalize(toEye + lightVec);
	float roughnessFactor = (m + 8.0f) * pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	
	// �������������߹�.
	float3 fresnelFactor = SchlickFresnel(mat.FresnelR0, halfVec, lightVec);
	float3 specAlbedo = fresnelFactor * roughnessFactor;

	// ��specAlbedo���Ʒ�Χ��[0, 1], ���ұ���ƽ��Ч��.
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

	// ������ + �߹ⷴ��.
	return (mat.DiffuseAlbedo.rgb + specAlbedo) * lightStrength;
}

///---------------------------------------------------------------------------------------
/// ����ƽ�й����.
///---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
	// ��������ȡ..
	float3 lightVec = -L.Direction;

	// ���������Ҷ����ȡ����ǿ��.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

///---------------------------------------------------------------------------------------
/// ������Դ�Ĺ���.
///---------------------------------------------------------------------------------------
float3 ComputePointLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// ���������, ���ﻹû�й�һ��.
	float3 lightVec = L.Position - pos;

	// ���Դ��Χ����.
	float d = length(lightVec);
	if (d > L.FalloffEnd)
		return 0.0f;

	// ��һ��������.
	lightVec /= d;

	// ���������Ҷ����ȡ����ǿ��.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	// ������Դ��˥��..
	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

///---------------------------------------------------------------------------------------
// ����۹�ƵĹ���.
///---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light L, Material mat, float3 pos, float3 normal, float3 toEye)
{
	// �͵��Դ����, �۹��Ҳ��Ҫλ����Ϣ�ͷ�Χ����.
	float3 lightVec = L.Position - pos;

	float d = length(lightVec);
	if (d > L.FalloffEnd)
		return 0.0f;

	lightVec /= d;

	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.Strength * ndotl;

	float att = CalcAttenuation(d, L.FalloffStart, L.FalloffEnd);
	lightStrength *= att;

	// �۹�Ƶľ۹�Ч��.
	float spotFactor = pow(max(dot(-lightVec, L.Direction), 0.0f), L.SpotPower);
	lightStrength *= spotFactor;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}

/// ͨ�õļ�����չ��ߺ���.
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
