#pragma once

#include "..\..\..\Common\d3dUtil.h"

class CubeRenderTarget
{
public:
	CubeRenderTarget(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
	CubeRenderTarget& operator=(const CubeRenderTarget& rhs) = delete;
	~CubeRenderTarget() = default;

	///<summary>
	/// 输出生成的立方体纹理资源.
	/// <returns>纹理资源.</returns>
	///</summary>
	ID3D12Resource* Resource() const;

	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv() const;

	D3D12_VIEWPORT Viewport() const;
	D3D12_RECT ScissorRect() const;

	///<summary>
	/// 将对应的描述符保存, 并创建对应的描述符资源.
	/// <param name="hCpuSrv">CPU端的描述符.</param>
	/// <param name="hGpuSrv">GPU端的描述符.</param>
	/// <param name="hCpuDsv">CPU端的描述符.</param>
	///</summary>
	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDsv);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildDescriptors();
	void BuildResources();

private:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewpot;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	Microsoft::WRL::ComPtr<ID3D12Resource> mShadowCubeMap = nullptr;
};

