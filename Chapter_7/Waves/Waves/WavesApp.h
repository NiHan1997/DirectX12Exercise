#pragma once

#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;
	~RenderItem() = default;

	int NumFramesDirty = gNumFrameResources;

	int ObjectCBIndex = -1;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	MeshGeometry* Geo = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

/// ��Ⱦ�ֲ�ʹ��.
enum class RenderLayer : int
{
	Opaque = 0,
	Count
};

class WavesApp : public D3DApp
{
public:
	WavesApp(HINSTANCE hInstance);
	WavesApp(const WavesApp& rhs) = delete;
	WavesApp& operator=(const WavesApp& rhs) = delete;
	~WavesApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWavesVB(const GameTimer& gt);	

	void BuidlRootSignature();
	void BuildShadersAndInputLayout();
	void BuildLandGeo();
	void BuildWavesGeo();
	void BuildRenderItem();
	void BuildFrameResources();
	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildPSOs();

	void DrawRenderItem(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z) const;
	XMFLOAT3 GetHillsNormal(float x, float z) const;

private:
	// ֡��Դ���.
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	// �����������������Ѻ͸�ǩ��.
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	UINT mPassCbvOffset = 0;

	// ��ɫ�����.
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// ����������.
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;

	// ��Ⱦ�����.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	RenderItem* mWavesRitem = nullptr;

	// PSO.
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	// ����.
	std::unique_ptr<Waves> mWaves = nullptr;

	// ����.
	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.1f;
	float mRadius = 50.0f;

	PassConstants mMainPassCB;

	POINT mLastMousePos;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		WavesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}