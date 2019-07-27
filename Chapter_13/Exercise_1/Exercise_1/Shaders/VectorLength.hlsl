
struct InputData
{
	float3 vect;
};

struct OutputData
{
	float vectorLength;
};

StructuredBuffer<InputData> gInput     : register(t0);
RWStructuredBuffer<OutputData> gOutput : register(u0);

[numthreads(64, 1, 1)]
void CS(int3 dispatchID : SV_DispatchThreadID)
{
	gOutput[dispatchID.x].vectorLength = length(gInput[dispatchID.x].vect);
}
