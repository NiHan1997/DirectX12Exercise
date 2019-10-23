#pragma once

#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\Camera.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"
#include "..\..\..\Common\GeometryGenerator.h"
#include "FrameResource.h"
#include "CubeRenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

// 阴影贴图的分辨率.
const UINT ShadowMapSize = 2048;

struct RenderItem
{
	RenderItem() = default;
	~RenderItem() = default;

	int ObjectCBIndex = -1;

	int NumFramesDirty = gNumFrameResources;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

enum class RenderLayer : int 
{
	Opaque = 0,
	Sky,
	Count
};

class ShadowCubeMapApp : public D3DApp
{
public:
	ShadowCubeMapApp(HINSTANCE hInstance);
	ShadowCubeMapApp(const ShadowCubeMapApp& rhs) = delete;
	ShadowCubeMapApp& operator=(const ShadowCubeMapApp& rhs) = delete;
	~ShadowCubeMapApp();

	virtual bool Initialize() override;

private:
	virtual void CreateRtvAndDsvDescriptorHeaps() override;
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCBs(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildPSOs();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuDsv(int index) const;
	void BuildShadowMapCamera(float x, float y, float z);
	void DrawSceneToShadowMap(ID3D12GraphicsCommandList* cmdList);
	
private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	RenderItem* mSkullRitem = nullptr;

	UINT mSkyTexHeapIndex = 0;
	UINT mShadowHeapIndex = 0;

	std::unique_ptr<CubeRenderTarget> mShadowMap = nullptr;

	PassConstants mMainPassCB;
	XMFLOAT3 mPointLightPos = { 0.0f, 6.0f, 0.0f };

	Camera mCamera;
	Camera mShadowCamera[6];				// 用于生成立方体阴影的6个摄像机.

	POINT mLastMousePos;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShadowCubeMapApp theApp(hInstance);
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