
struct Data
{
	float3 v1;
	float2 v2;
};

StructuredBuffer<Data> gInputA   : register(t0);
StructuredBuffer<Data> gInputB   : register(t1);
RWStructuredBuffer<Data> gOutput : register(u0);

[numthreads(32, 1, 1)]
void CS(int3 dispatchTreadID : SV_DispatchThreadID)
{
	gOutput[dispatchTreadID.x].v1 = gInputA[dispatchTreadID.x].v1 + gInputB[dispatchTreadID.x].v1;
	gOutput[dispatchTreadID.x].v2 = gInputA[dispatchTreadID.x].v2 + gInputB[dispatchTreadID.x].v2;
}