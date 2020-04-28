#include "RenderTarget.h"

RenderTarget::RenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mFormat = format;

	BuildResources();
}

ID3D12Resource* RenderTarget::Resource() const
{
	return mOffScreenTexture.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE RenderTarget::GpuSrv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE RenderTarget::CpuRtv() const
{
	return mhCpuRtv;
}

void RenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv)
{
	mhCpuSrv = hCpuSrv;
	mhCpuRtv = hCpuRtv;

	mhGpuSrv = hGpuSrv;

	BuildResources();
	BuildDescriptors();
}

void RenderTarget::OnResize(UINT newWidth, UINT newHeight)
{
	if (mWidth != newWidth || mHeight != newHeight)
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();
		BuildDescriptors();
	}
}

void RenderTarget::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Format = mFormat;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(mOffScreenTexture.Get(), &srvDesc, mhCpuSrv);

	md3dDevice->CreateRenderTargetView(mOffScreenTexture.Get(), nullptr, mhCpuRtv);
}

void RenderTarget::BuildResources()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Format = mFormat;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	float clearColor[] = { 0.690196097f, 0.768627524f, 0.870588303f, 1.000000000f };
	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mFormat;
	optClear.Color[0] = clearColor[0];
	optClear.Color[1] = clearColor[1];
	optClear.Color[2] = clearColor[2];
	optClear.Color[3] = clearColor[3];

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(mOffScreenTexture.ReleaseAndGetAddressOf())));
}
