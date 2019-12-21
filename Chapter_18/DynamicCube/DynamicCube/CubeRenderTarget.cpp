#include "CubeRenderTarget.h"

CubeRenderTarget::CubeRenderTarget(ID3D12Device * device, UINT width, UINT height, DXGI_FORMAT format)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mFormat = format;

	mViewpot = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (LONG)width, (LONG)height };

	BuildResources();
}

ID3D12Resource * CubeRenderTarget::Resource() const
{
	return mCubeMap.Get();
}

CD3DX12_GPU_DESCRIPTOR_HANDLE CubeRenderTarget::Srv() const
{
	return mhGpuSrv;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE CubeRenderTarget::Rtv(int faceIndex) const
{
	return mhCpuRtv[faceIndex];
}

D3D12_VIEWPORT CubeRenderTarget::Viewport() const
{
	return mViewpot;
}

D3D12_RECT CubeRenderTarget::ScissorRect() const
{
	return mScissorRect;
}

void CubeRenderTarget::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6])
{
	mhCpuSrv = hCpuSrv;
	mhGpuSrv = hGpuSrv;
	
	for (int i = 0; i < 6; ++i)
		mhCpuRtv[i] = hCpuRtv[i];

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
	srvDesc.Format = mFormat;
	srvDesc.TextureCube.MipLevels = 1;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(mCubeMap.Get(), &srvDesc, mhCpuSrv);

	// 创建Rtv.
	for (int i = 0; i < 6; ++i)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		rtvDesc.Format = mFormat;
		rtvDesc.Texture2DArray.MipSlice = 0;
		rtvDesc.Texture2DArray.PlaneSlice = 0;
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		rtvDesc.Texture2DArray.ArraySize = 1;

		md3dDevice->CreateRenderTargetView(mCubeMap.Get(), &rtvDesc, mhCpuRtv[i]);
	}
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
		IID_PPV_ARGS(mCubeMap.GetAddressOf())));
}
