#include "SobelFilter.h"

SobelFilter::SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	md3dDevice = device;
	mWdith = width;
	mHeight = height;
	mFormat = format;

	BuildResources();
}

ID3D12Resource* SobelFilter::Resource() const
{
	return mOutput.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SobelFilter::OutputGpuSrv() const
{
	return mhGpuSrv;
}

UINT SobelFilter::DescriptorCount() const
{
	return 4;
}

void SobelFilter::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc, UINT descriptorSize)
{
	mhCpuSrv = hCpuDesc;
	mhCpuUav = hCpuDesc.Offset(1, descriptorSize);

	mhGpuSrv = hGpuDesc;
	mhGpuUav = hGpuDesc.Offset(1, descriptorSize);

	BuildDescriptors();
}

void SobelFilter::OnResize(UINT newWidth, UINT newHeight)
{
	if (mWdith != newWidth || mHeight != newHeight)
	{
		mWdith = newWidth;
		mHeight = newHeight;

		BuildResources();
		BuildDescriptors();
	}
}

void SobelFilter::Execute(ID3D12GraphicsCommandList* cmdList, 
	ID3D12RootSignature* rootSig, 
	ID3D12PipelineState* pso, 
	CD3DX12_GPU_DESCRIPTOR_HANDLE input)
{
	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	cmdList->SetComputeRootDescriptorTable(0, input);
	cmdList->SetComputeRootDescriptorTable(2, mhGpuUav);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	UINT numGroupX = (UINT)ceilf(mWdith / 256.0f);
	//UINT numGroupY = (UINT)ceilf(mHeight / 16.0f);
	cmdList->Dispatch(numGroupX, (UINT)ceilf(mHeight), 1);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutput.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SobelFilter::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFormat;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Format = mFormat;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mOutput.Get(), &srvDesc, mhCpuSrv);
	md3dDevice->CreateUnorderedAccessView(mOutput.Get(), nullptr, &uavDesc, mhCpuUav);
}

void SobelFilter::BuildResources()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.Format = mFormat;
	texDesc.Width = mWdith;
	texDesc.Height = mHeight;
	texDesc.MipLevels = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mOutput.GetAddressOf())));
}
