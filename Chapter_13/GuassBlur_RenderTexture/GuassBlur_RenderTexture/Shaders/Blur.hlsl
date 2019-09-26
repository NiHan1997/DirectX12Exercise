
/// ��˹ģ������ĳ�������.
cbuffer cbSettings : register(b0)
{
	int gBlurRadius;			// ��˹ģ���뾶.

	// ��˹ģ����Ȩ��, ���뾶Ϊ5, �����ЩȨ�ز�һ��ȫ��������.
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

/// ������ɫ����Ҫ��������Դ.
Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// ���ģ���뾶.
static const int gMaxBlurRadius = 5;

// �����ڴ�.
#define N 256
#define CacheSize (N + 2 * gMaxBlurRadius)
groupshared float4 gCache[CacheSize];

/// ����ģ���ļ�����ɫ��.
[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispathThreadID : SV_DispatchThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

	// �������ڴ���߽�.
	if (groupThreadID.x < gBlurRadius)
	{
		int x = max(dispathThreadID.x - gBlurRadius, 0);
		gCache[groupThreadID.x] = gInput[int2(x, dispathThreadID.y)];
	}

	// �������ڴ��ұ߽�.
	if (groupThreadID.x >= N - gBlurRadius)
	{
		int x = min(dispathThreadID.x + gBlurRadius, gInput.Length.x - 1);
		gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispathThreadID.y)];
	}

	// �����ڴ��м��������.
	gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispathThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	// �����ش���.
	float4 blurColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.x + gBlurRadius + i;
		blurColor += weights[i + gBlurRadius] * gCache[k];
	}

	gOutput[dispathThreadID.xy] = blurColor;
}

/// ����ģ��������ɫ��.
[numthreads(1, N, 1)]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID, int3 dispathThreadID : SV_DispatchThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

	// �������ڴ���ϱ߽�.
	if (groupThreadID.y < gBlurRadius)
	{
		int y = max(dispathThreadID.y - gBlurRadius, 0);
		gCache[groupThreadID.y] = gInput[int2(dispathThreadID.x, y)];
	}

	// �������ڴ���±߽�.
	if (groupThreadID.y >= N - gBlurRadius)
	{
		int y = min(dispathThreadID.y + gBlurRadius, gInput.Length.y - 1);
		gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispathThreadID.x, y)];
	}

	// �������ڴ���������.
	gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispathThreadID.xy, gInput.Length.xy - 1)];

	GroupMemoryBarrierWithGroupSync();

	// �����ش���.
	float4 blurColor = float4(0.0, 0.0, 0.0, 0.0);
	for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
	{
		int k = groupThreadID.y + gBlurRadius + i;
		blurColor += weights[i + gBlurRadius] * gCache[k];
	}

	gOutput[dispathThreadID.xy] = blurColor;
}