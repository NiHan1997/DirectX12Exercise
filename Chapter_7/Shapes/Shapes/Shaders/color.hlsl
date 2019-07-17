
/// 每个物体的常量缓冲区结构体.
struct ObjectConstants
{
	float4x4 gWorld;
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

	float4 posW = mul(float4(vin.PosL, 1.0), gObjectConstants.gWorld);
	vout.PosH = mul(posW, gPassConstants.gViewProj);

	vout.Color = vin.Color;

	return vout;
}

/// 像素着色器.
float4 PS (VertexOut pin) : SV_Target
{
	return pin.Color;
}