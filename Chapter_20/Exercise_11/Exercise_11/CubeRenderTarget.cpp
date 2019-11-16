#include "CubeRenderTarget.h"

CubeRenderTarget::CubeRenderTarget(ID3D12Device * device, UINT width, UINT height)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;

	mViewpot = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (LONG)width, (LONG)height };

	BuildResources();
}

ID3D12Resource * CubeRenderTarget::Resource() const
{
	return mShadowCubeMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::Srv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Dsv() const
{
	return mhCpuDsv;
}

D3D12_VIEWPORT CubeRenderTarget::Viewport() const
{
	return mViewpot;
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
	return mScissorRect;
}

void CubeRenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv)
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;	
	mhCpuDsv = hCpuDsv;

	BuildDescriptors();
}

void CubeRenderTarget::OnResize(UINT newWidth, UINT newHeight)
{
	if (mWidth != newWidth || mHeight != newHeight)
	{
		mWidth = newWidth;
		mHeight = newHeight;

		BuildResources();
		BuildDescriptors();
	}
}

void CubeRenderTarget::BuildDescriptors()
{
	// 创建Srv.
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(mShadowCubeMap.Get(), &srvDesc, mhCpuSrv);

	// 创建Dsv.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvDesc.Texture2DArray.ArraySize = 6;
	dsvDesc.Texture2DArray.FirstArraySlice = 0;
	dsvDesc.Texture2DArray.MipSlice = 0;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	md3dDevice->CreateDepthStencilView(mShadowCubeMap.Get(), &dsvDesc, mhCpuDsv);
}

void CubeRenderTarget::BuildResources()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 6;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.Format = mFormat;
	texDesc.MipLevels = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mShadowCubeMap.GetAddressOf())));
}
