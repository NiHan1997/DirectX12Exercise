
/// 每个物体的常量缓冲区结构体.
struct ObjectConstants
{
	float gWorld0;
	float gWorld1;
	float gWorld2;
	float gWorld3;
	float gWorld4;
	float gWorld5;
	float gWorld6;
	float gWorld7;
	float gWorld8;
	float gWorld9;
	float gWorld10;
	float gWorld11;
	float gWorld12;
	float gWorld13;
	float gWorld14;
	float gWorld15;
};

/// 过程常量缓冲区结构体.
struct PassConstants
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;
	float gTotalTime;
	float gDeltaTime;
};

/// 常量缓冲区定义.
ConstantBuffer<ObjectConstants> gObjectConstants : register(b0);
ConstantBuffer<PassConstants> gPassConstants 	 : register(b1);

/// 顶点着色器输入.
struct VertexIn
{
	float3 PosL  : POSITION;
	float4 Color : COLOR;
};

/// 顶点着色器输出, 像素着色器输入.
struct VertexOut
{
	float4 PosH  : SV_POSITION;
	float4 Color : COLOR;
};


/// 顶点着色器.
VertexOut VS (VertexIn vin)
{
	VertexOut vout;

	float4x4 world = float4x4(
		float4(gObjectConstants.gWorld0, gObjectConstants.gWorld1, gObjectConstants.gWorld2, gObjectConstants.gWorld3),
		float4(gObjectConstants.gWorld4, gObjectConstants.gWorld5, gObjectConstants.gWorld6, gObjectConstants.gWorld7),
		float4(gObjectConstants.gWorld8, gObjectConstants.gWorld9, gObjectConstants.gWorld10, gObjectConstants.gWorld11),
		float4(gObjectConstants.gWorld12, gObjectConstants.gWorld13, gObjectConstants.gWorld14, gObjectConstants.gWorld15));

	float4 posW = mul(float4(vin.PosL, 1.0), world);
	vout.PosH = mul(posW, gPassConstants.gViewProj);

	vout.Color = vin.Color;

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	return pin.Color;
}