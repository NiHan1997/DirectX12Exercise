#pragma once
#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"
#include "FrameResource.h"
#include <time.h>
#include <iomanip>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct InputData
{
	XMFLOAT3 vect;
};

struct OutputData
{
	float length;
};

class VectorLengthApp : public D3DApp
{
public:
	VectorLengthApp(HINSTANCE hInstance);
	VectorLengthApp(const VectorLengthApp& rhs) = delete;
	VectorLengthApp& operator=(const VectorLengthApp& rhs) = delete;
	~VectorLengthApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	void BuildBuffers();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();

	void DoComputeWork();

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	const int NumDataElements = 64;

	std::vector<InputData> mInputData;

	ComPtr<ID3D12Resource> mInputBuffer = nullptr;
	ComPtr<ID3D12Resource> mInputUploadBuffer = nullptr;
	ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
	ComPtr<ID3D12Resource> mReadBackBuffer = nullptr;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		VectorLengthApp theApp(hInstance);
		if (theApp.Initialize() == false)
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}