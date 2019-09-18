///***********************************************************************************
/// Sobel��Ե���.
///***********************************************************************************

Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// �����ڴ��Ż�����.
static const int sampleRadius = 1;
#define N 256
#define CacheSize ((N + 2 * sampleRadius) * (2 * sampleRadius + 1))
groupshared float4 gCache[CacheSize];

/// �������ص�����ֵ(�Ҷ�ֵ).
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(N, 1, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID, int3 groupThreadID : SV_GroupThreadID)
{
	// �����ڴ�����������.
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

	// �����ڴ��ұ��������.
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

	// �����ڴ���������.
	[unroll]
	for (int y = 0; y < 2 * sampleRadius + 1; ++y)
	{
		gCache[groupThreadID.x + sampleRadius + y * (N + 2 * sampleRadius)] =
			gInput[min(int2(dispatchThreadID.xy.x, dispatchThreadID.xy.y + y - 1), gInput.Length.xy - 1)];
	}

	GroupMemoryBarrierWithGroupSync();


	// �������.
	float4 c[3][3];
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			c[i][j] = gCache[groupThreadID.x + j + i * (N + 2 * sampleRadius)];
		}
	}

	// ����x������ݶ�.
	float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] +
		1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];

	// ����y������ݶ�.
	float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][1] +
		1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];

	// ���������ݶ�, ������abs(Gx) + abs(Gy)�����, ����Ч������ô��, �������ܿ�������.
	//float4 mag = sqrt(Gx * Gx + Gy * Gy);
	float4 mag = abs(Gx) + abs(Gy);

	// ����Ե���Ϊ��ɫ, �����ط����Ϊ��ɫ.
	mag = 1.0f - saturate(CalcLuminance(mag.rgb));

	gOutput[dispatchThreadID.xy] = mag;
}
