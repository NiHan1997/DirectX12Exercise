
/// 高斯模糊必需的常量数据.
cbuffer cbSettings : register(b0)
{
	int gBlurRadius;			// 高斯模糊半径.

	// 高斯模糊的权重, 最大半径为5, 因此这些权重不一定全部会用完.
	float w0;
	float w1;
	float w2;
	float w3;
	float w4;
	float w5;
	float w6;
	float w7;
	float w8;
	float w9;
	float w10;
};

/// 计算着色器需要的纹理资源.
Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 最大模糊半径.
static const int gMaxBlurRadius = 5;

// 共享内存.
#define N 256
#define CacheSize (N + 2 * gMaxBlurRadius)
groupshared float4 gCache[CacheSize];

/// 横向模糊的计算着色器.
[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispathThreadID : SV_DispatchThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

	// 处理共享内存左边界.
	if (groupThreadID.x < gBlurRadius)
	{
		int x = max(dispathThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[int2(x, dispathThreadID.y)];
	}

	// 处理共享内存右边界.
	if (groupThreadID.x >= N - gBlurRadius)
	{
		int x = min(dispathThreadID.x + gBlurRadius, gInput.Length.x - 1);
		gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispathThreadID.y)];
	}

	// 共享内存中间区域填充.
	gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispathThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	// 逐像素处理.
	float4 blurColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.x + gBlurRadius + i;
		blurColor += weights[i + gBlurRadius] * gCache[k];
	}

	gOutput[dispathThreadID.xy] = blurColor;
}

/// 纵向模糊计算着色器.
[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispathThreadID : SV_DispatchThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

	// 处理共享内存的上边界.
	if (groupThreadID.y < gBlurRadius)
	{
		int y = max(dispathThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[int2(dispathThreadID.x, y)];
	}

	// 处理共享内存的下边界.
	if (groupThreadID.y >= N - gBlurRadius)
	{
		int y = min(dispathThreadID.y + gBlurRadius, gInput.Length.y - 1);
		gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispathThreadID.x, y)];
	}

	// 处理共享内存中央区域.
	gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispathThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	// 逐像素处理.
	float4 blurColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.y + gBlurRadius + i;
		blurColor += weights[i + gBlurRadius] * gCache[k];
	}

	gOutput[dispathThreadID.xy] = blurColor;
}