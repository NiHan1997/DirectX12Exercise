#pragma once

#include "..\..\..\Common\d3dUtil.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

class SSAO
{
public:
	SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList,UINT width, UINT height);
	SSAO(const SSAO& rhs) = delete;
	SSAO& operator=(const SSAO& rhs) = delete;
	~SSAO() = default;

	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
	static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static const int MaxBlurRadius = 5;

	UINT SSAOMapWidth() const;
	UINT SSAOMapHeight() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);
	std::vector<float> CalcGaussWeights(float sigma);

	ID3D12Resource* AmbientMap() const;
	ID3D12Resource* NormalMap() const;

	CD3DX12_CPU_DESCRIPTOR_HANDLE NormalMapCPURtv() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE NormalMapGPUSrv() const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE AmbientMapGPUSrv() const;

	void BuildDescriptors(
		ID3D12Resource* depthStencilBuffer,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		UINT cbvSrvUavDescSize,
		UINT rtvDescSize);

	void RebuildDescriptors(ID3D12Resource* depthStencilBuffer);

	void SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso);

	void OnResize(UINT newWidth, UINT newHeight);

	void ComputeSSAO(ID3D12GraphicsCommandList* cmdList, FrameResource* currentFrame, int blurCount);

private:
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currentFrame, int blurCount);
	void BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur);

	void BuildResources();
	void BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList);

	void BuildOffsetVectors();

private:
	ID3D12Device* md3dDevice = nullptr;

	ComPtr<ID3D12RootSignature> mSSAORootSig = nullptr;

	ID3D12PipelineState* mSSAOPso = nullptr;
	ID3D12PipelineState* mSSAOBlurPso = nullptr;

	ComPtr<ID3D12Resource> mRandomVectorMap = nullptr;
	ComPtr<ID3D12Resource> mRandomVectorMapUploadBuffer = nullptr;
	ComPtr<ID3D12Resource> mNormalMap = nullptr;
	ComPtr<ID3D12Resource> mAmbientMap0 = nullptr;
	ComPtr<ID3D12Resource> mAmbientMap1 = nullptr;

	// 法线纹理描述符.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;

	// 深度纹理描述符.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;

	// 随机向量纹理描述符.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;

	// SSAO需要的描述符.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap1GpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap1CpuRtv;

	UINT mRenderTargetWidth = 0;
	UINT mRenderTargetHeight = 0;

	DirectX::XMFLOAT4 mOffsets[14];

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
};