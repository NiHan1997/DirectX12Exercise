#pragma once

#include "..\..\..\Common\d3dUtil.h"

class RenderTarget
{
public:
	RenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	RenderTarget(const RenderTarget& rhs) = delete;
	RenderTarget& operator=(const RenderTarget& rhs) = delete;
	~RenderTarget() = default;

	ID3D12Resource* Resource() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GpuSrv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE CpuRtv() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildDescriptors();
	void BuildResources();

private:
	ID3D12Device* md3dDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mOffScreenTexture = nullptr;
};

