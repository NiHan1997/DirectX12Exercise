#include "GpuWaves.h"
#include <vector>
#include <algorithm>
#include <cassert>

GpuWaves::GpuWaves(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, 
	int m, int n, float dx, float dt, float speed, float damping)
{
	md3dDevice = device;

	mNumRows = m;
	mNumColumns = n;

	assert(m * n % 256 == 0);

	mVertexCount = m * n;
	mTriangleCount = (m - 1) * (n - 1) * 2;

	mTimeStep = dt;
	mSpatialStep = dx;

	float d = damping * dt + 2.0f;
	float e = (speed * speed) * (dt * dt) / (dx * dx);

	mK[0] = (damping * dt - 2.0f) / d;
	mK[1] = (4.0f - 8.0f * e) / d;
	mK[2] = 2 * e / d;

	BuildResources(cmdList);
}

UINT GpuWaves::RowCount() const
{
	return mNumRows;
}

UINT GpuWaves::ColumnCount() const
{
	return mNumColumns;
}

UINT GpuWaves::VertexCount() const
{
	return mVertexCount;
}

UINT GpuWaves::TriangleCount() const
{
	return mTriangleCount;
}

float GpuWaves::Width() const
{
	return mNumColumns * mSpatialStep;
}

float GpuWaves::Depth() const
{
	return mNumRows * mSpatialStep;
}

float GpuWaves::SpatialStep() const
{
	return mSpatialStep;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE GpuWaves::DisplacementMap() const
{
	return mCurrSolSrv;
}

UINT GpuWaves::DescriptorCount() const
{
	return 6;
}

void GpuWaves::BuildResources(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.Width = mNumRows;
	texDesc.Height = mNumColumns;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(mPrevSol.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(mCurrSol.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(mNextSol.GetAddressOf())));


	// 为了将CPU端数据上传到GPU, 需要上传堆.
	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uoloadBufferByteSize = GetRequiredIntermediateSize(mCurrSol.Get(), 0, num2DSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uoloadBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mPrevUploadBuffer.GetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uoloadBufferByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mCurrUploadBuffer.GetAddressOf())));

	std::vector<float> initData(mNumColumns * mNumRows, 0.0f);

	D3D12_SUBRESOURCE_DATA subResources = {};
	subResources.pData = initData.data();
	subResources.RowPitch = mNumColumns * sizeof(float);
	subResources.SlicePitch = subResources.RowPitch;

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevSol.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mPrevSol.Get(), mPrevUploadBuffer.Get(), 0, 0, num2DSubresources, &subResources);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mPrevSol.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSol.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mCurrSol.Get(), mCurrUploadBuffer.Get(), 0, 0, num2DSubresources, &subResources);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSol.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
	BeforeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mNextSol.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void GpuWaves::BuillDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
	UINT descriptorSize)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	md3dDevice->CreateShaderResourceView(mPrevSol.Get(), &srvDesc, hCpuDescriptor);
	md3dDevice->CreateShaderResourceView(mCurrSol.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateShaderResourceView(mNextSol.Get(), &srvDesc, hCpuDescriptor.Offset(1, descriptorSize));

	md3dDevice->CreateUnorderedAccessView(mPrevSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mCurrSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));
	md3dDevice->CreateUnorderedAccessView(mNextSol.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));

	mPrevSolSrv = hGpuDescriptor;
	mCurrSolSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mNextSolSrv = hGpuDescriptor.Offset(1, descriptorSize);
	mPrevSolUav = hGpuDescriptor.Offset(1, descriptorSize);
	mCurrSolUav = hGpuDescriptor.Offset(1, descriptorSize);
	mNextSolUav = hGpuDescriptor.Offset(1, descriptorSize);
}

void GpuWaves::Update(const GameTimer& gt, ID3D12GraphicsCommandList* cmdList,
	ID3D12RootSignature* rootSig, ID3D12PipelineState* pso)
{
	static float t_base = 0.0f;
	t_base += gt.DeltaTime();

	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	if (t_base >= mTimeStep)
	{
		cmdList->SetComputeRoot32BitConstants(0, 3, mK, 0);

		cmdList->SetComputeRootDescriptorTable(1, mPrevSolUav);
		cmdList->SetComputeRootDescriptorTable(2, mCurrSolUav);
		cmdList->SetComputeRootDescriptorTable(3, mNextSolUav);

		UINT numGroupX = mNumColumns / 16;
		UINT numGroupY = mNumRows / 16;
		cmdList->Dispatch(numGroupX, numGroupY, 1);

		auto resTemp = mPrevSol;
		mPrevSol = mCurrSol;
		mCurrSol = mNextSol;
		mNextSol = resTemp;

		auto srvTemp = mPrevSolSrv;
		mPrevSolSrv = mCurrSolSrv;
		mCurrSolSrv = mNextSolSrv;
		mNextSolSrv = srvTemp;

		auto uavTemp = mPrevSolUav;
		mPrevSolUav = mCurrSolUav;
		mCurrSolUav = mNextSolUav;
		mNextSolUav = uavTemp;

		t_base = 0.0f;
		if (BeforeState != D3D12_RESOURCE_STATE_GENERIC_READ)
		{
			cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSol.Get(),
				BeforeState, D3D12_RESOURCE_STATE_GENERIC_READ));
			BeforeState = D3D12_RESOURCE_STATE_GENERIC_READ;
		}
	}
}

void GpuWaves::Disturb(ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, 
	ID3D12PipelineState* pso, UINT i, UINT j, float magnitude)
{
	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	UINT disturbTex[2] = { j, i };

	cmdList->SetComputeRoot32BitConstants(0, 1, &magnitude, 3);
	cmdList->SetComputeRoot32BitConstants(0, 2, disturbTex, 4);

	cmdList->SetComputeRootDescriptorTable(3, mCurrSolUav);

	if (BeforeState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mCurrSol.Get(),
			BeforeState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
		BeforeState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}

	cmdList->Dispatch(1, 1, 1);
}
