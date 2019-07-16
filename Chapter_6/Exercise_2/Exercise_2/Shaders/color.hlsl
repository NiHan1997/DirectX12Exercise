
/// ���������ݷ�װΪһ���ṹ�巽��ʹ��, Shader Model 5.1 ���﷨.
struct ObjectConstants
{
	float4x4 gWorldViewProj;
};

/// ��������������.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);

/// ������ɫ������.
struct VertexIn
{
	float3 PosL : POSITION;
	float4 Color : COLOR;
};

/// ������ɫ�����, ������ɫ������.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 Color : TEXCOORD0;
};

/// ������ɫ��.
VertexOut VS(VertexIn vin)
{
	VertexOut vout;
	vout.PosH = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorldViewProj);
	vout.Color = vin.Color;

	return vout;
}

/// ������ɫ��.
float4 PS(VertexOut pin) : SV_Target
{
	return pin.Color;
}