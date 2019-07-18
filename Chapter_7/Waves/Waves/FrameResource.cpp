#include "FrameResource.h"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT waveVertexCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
	
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, objCount, true);
	PassCB = std::make_unique<UploadBuffer<PassConstants>>(device, passCount, true);
	WavesVB = std::make_unique<UploadBuffer<Vertex>>(device, waveVertexCount, false);
}

FrameResource::~FrameResource()
{
}
