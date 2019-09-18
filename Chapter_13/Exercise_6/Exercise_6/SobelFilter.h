#pragma once

#include "..\..\..\Common\d3dUtil.h"

class SobelFilter
{
public:
	SobelFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	SobelFilter(const SobelFilter& rhs) = delete;
	SobelFilter& operator=(const SobelFilter& rhs) = delete;
	~SobelFilter() = default;

	ID3D12Resource* Resource() const;

	CD3DX12_GPU_DESCRIPTOR_HANDLE OutputGpuSrv() const;

	UINT DescriptorCount() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc,
		UINT descriptorSize);

	void OnResize(UINT newWidth, UINT newHeight);

	/// 注意这里的输入input是值类型.
	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* pso,
		CD3DX12_GPU_DESCRIPTOR_HANDLE input);

private:
	void BuildDescriptors();
	void BuildResources();

private:
	ID3D12Device* md3dDevice = nullptr;
	UINT mWdith = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuUav;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOutput = nullptr;
};

