#pragma once

#include "..\..\..\Common\d3dUtil.h"

class BlurFilter
{
public:
	///<summary>
	/// 在以后这是一种常用的写法, 这里就是讲功能分离出来.
	/// <param name="device">当前设备接口, 在这里需要使用ID3D12Device创建资源.</param>
	/// <param name="width">需要创建纹理资源, 纹理资源的宽度.</param>
	/// <param name="height">纹理资源的高度度.</param>
	/// <param name="format">纹理资源的类型.</param>
	///</summary>
	BlurFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	BlurFilter(const BlurFilter& rhs) = delete;
	BlurFilter& operator=(const BlurFilter& rhs) = delete;
	~BlurFilter() = default;

	///<summary>
	/// 这里是模糊后输出的纹理资源.
	/// <returns>纹理资源.</returns>
	///</summary>
	ID3D12Resource* Output() const;

	///<summary>
	/// 将对应的描述符保存, 并创建对应的描述符资源.
	/// <param name="hCpuSrv">CPU端的描述符.</param>
	/// <param name="hGpuSrv">GPU端的描述符.</param>
	/// <param name="descriptorSize">描述符的大小.</param>
	///</summary>
	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		UINT descriptorSize);

	///<summary>
	/// 如果窗口大小改变了, 纹理资源的相关数据也会改变.
	/// <param name="newWidth">新的宽度.</param>
	/// <param name="newHeight">新的高度.</param>
	///</summary>
	void OnResize(UINT newWidth, UINT newHeight);

	///<summary>
	/// 执行模糊操作.
	/// <param name="cmdList">执行模糊所用的命令列表.</param>
	/// <param name="rootSig">执行模糊操作相关的计算根签名.</param>
	/// <param name="horzBlurPso">横向模糊的PSO.</param>
	/// <param name="vertBlurPso">纵向模糊的PSO.</param>
	/// <param name="input">输入的后台缓冲区数据.</param>
	/// <param name="blurCount">模糊次数.</param>
	///</summary>
	void Execute(
		ID3D12GraphicsCommandList* cmdList,
		ID3D12RootSignature* rootSig,
		ID3D12PipelineState* horzBlurPso,
		ID3D12PipelineState* vertBlurPso,
		ID3D12Resource* input,
		int blurCount);

private:
	///<summary>
	/// 计算高斯模糊的权重.
	/// <param name="sigma">高斯模糊的必须参数.</param>
	/// <returns>每个像素的权重集合.</returns>
	///</summary>
	std::vector<float> CalcBlurWeights(float sigma);

	///<summary>
	/// 为对应的描述符创建SRV和UAV.
	///</summary>
	void BuildDescriptors();

	///<summary>
	/// 创建纹理资源.
	///</summary>
	void BuildResources();

private:
	const int MaxBlurRadius = 5;				// 最大模糊范围.

	// 构造函数对应的相关参数.
	ID3D12Device* md3dDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 相关的描述符资源.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuUav;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuUav;

	// 纹理资源.
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0 = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap1 = nullptr;
};
