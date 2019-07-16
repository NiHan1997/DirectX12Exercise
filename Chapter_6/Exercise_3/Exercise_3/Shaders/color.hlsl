
/// 将常量数据封装为一个结构体方便使用, Shader Model 5.1 的语法.
struct ObjectConstants
{
	float4x4 gWorldViewProj;
};

/// 常量缓冲区数据.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL : POSITION;
	float4 Color : COLOR;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH : SV_POSITION;
	float4 Color : TEXCOORD0;
};

/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;
	vout.PosH = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorldViewProj);
	vout.Color = vin.Color;

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	return pin.Color;
}