///***********************************************************************************
/// Sobel边缘检测.
///***********************************************************************************

Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 共享内存优化程序.
static const int sampleRadius = 1;
#define N 256
#define CacheSize ((N + 2 * sampleRadius) * (2 * sampleRadius + 1))
groupshared float4 gCache[CacheSize];

/// 计算像素的亮度值(灰度值).
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(N, 1, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID, int3 groupThreadID : SV_GroupThreadID)
{
	// 共享内存左边区域填充.
	if (groupThreadID.x < sampleRadius)
	{
		[unroll]
		for (int y = 0; y < 2 * sampleRadius + 1; ++y)
		{
			int x = max(dispatchThreadID.x - sampleRadius, 0);
			gCache[groupThreadID.x + y * (N + 2 * sampleRadius)] = 
				gInput[int2(x, dispatchThreadID.y + y - 1)];
		}
	}

	// 共享内存右边区域填充.
	if (groupThreadID.x >= N - sampleRadius)
	{
		[unroll]
		for (int y = 0; y < 2 * sampleRadius + 1; ++y)
		{
			int x = min(dispatchThreadID.x + sampleRadius, gInput.Length.x - 1);
			gCache[groupThreadID.x + 2 * sampleRadius + y * (N + 2 * sampleRadius)] =
				gInput[int2(x, dispatchThreadID.y + y - 1)];
		}
	}

	// 共享内存中央区域.
	[unroll]
	for (int y = 0; y < 2 * sampleRadius + 1; ++y)
	{
		gCache[groupThreadID.x + sampleRadius + y * (N + 2 * sampleRadius)] =
			gInput[min(int2(dispatchThreadID.xy.x, dispatchThreadID.xy.y + y - 1), gInput.Length.xy - 1)];
	}

	GroupMemoryBarrierWithGroupSync();


	// 纹理采样.
	float4 c[3][3];
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			c[i][j] = gCache[groupThreadID.x + j + i * (N + 2 * sampleRadius)];
		}
	}

	// 计算x方向的梯度.
	float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] +
		1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];

	// 计算y方向的梯度.
	float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][1] +
		1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];

	// 计算整体梯度, 可以用abs(Gx) + abs(Gy)简单替代, 但是效果不那么好, 但是性能可以提升.
	//float4 mag = sqrt(Gx * Gx + Gy * Gy);
	float4 mag = abs(Gx) + abs(Gy);

	// 将边缘标记为黑色, 其他地方标记为白色.
	mag = 1.0f - saturate(CalcLuminance(mag.rgb));

	gOutput[dispatchThreadID.xy] = mag;
}
