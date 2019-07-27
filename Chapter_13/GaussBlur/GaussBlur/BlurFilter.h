#pragma once

#include "..\..\..\Common\d3dUtil.h"

class BlurFilter
{
public:
	///<summary>
	/// ���Ժ�����һ�ֳ��õ�д��, ������ǽ����ܷ������.
	/// <param name="device">��ǰ�豸�ӿ�, ��������Ҫʹ��ID3D12Device������Դ.</param>
	/// <param name="width">��Ҫ����������Դ, ������Դ�Ŀ��.</param>
	/// <param name="height">������Դ�ĸ߶ȶ�.</param>
	/// <param name="format">������Դ������.</param>
	///</summary>
	BlurFilter(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	BlurFilter(const BlurFilter& rhs) = delete;
	BlurFilter& operator=(const BlurFilter& rhs) = delete;
	~BlurFilter() = default;

	///<summary>
	/// ������ģ���������������Դ.
	/// <returns>������Դ.</returns>
	///</summary>
	ID3D12Resource* Output() const;

	///<summary>
	/// ����Ӧ������������, ��������Ӧ����������Դ.
	/// <param name="hCpuSrv">CPU�˵�������.</param>
	/// <param name="hGpuSrv">GPU�˵�������.</param>
	/// <param name="descriptorSize">�������Ĵ�С.</param>
	///</summary>
	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		UINT descriptorSize);

	///<summary>
	/// ������ڴ�С�ı���, ������Դ���������Ҳ��ı�.
	/// <param name="newWidth">�µĿ��.</param>
	/// <param name="newHeight">�µĸ߶�.</param>
	///</summary>
	void OnResize(UINT newWidth, UINT newHeight);

	///<summary>
	/// ִ��ģ������.
	/// <param name="cmdList">ִ��ģ�����õ������б�.</param>
	/// <param name="rootSig">ִ��ģ��������صļ����ǩ��.</param>
	/// <param name="horzBlurPso">����ģ����PSO.</param>
	/// <param name="vertBlurPso">����ģ����PSO.</param>
	/// <param name="input">����ĺ�̨����������.</param>
	/// <param name="blurCount">ģ������.</param>
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
	/// �����˹ģ����Ȩ��.
	/// <param name="sigma">��˹ģ���ı������.</param>
	/// <returns>ÿ�����ص�Ȩ�ؼ���.</returns>
	///</summary>
	std::vector<float> CalcBlurWeights(float sigma);

	///<summary>
	/// Ϊ��Ӧ������������SRV��UAV.
	///</summary>
	void BuildDescriptors();

	///<summary>
	/// ����������Դ.
	///</summary>
	void BuildResources();

private:
	const int MaxBlurRadius = 5;				// ���ģ����Χ.

	// ���캯����Ӧ����ز���.
	ID3D12Device* md3dDevice = nullptr;
	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	// ��ص���������Դ.
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur0CpuUav;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mBlur1CpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur0GpuUav;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mBlur1GpuUav;

	// ������Դ.
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap0 = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> mBlurMap1 = nullptr;
};
