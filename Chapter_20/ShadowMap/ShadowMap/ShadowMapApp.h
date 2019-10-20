#pragma once

#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\Camera.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"
#include "..\..\..\Common\GeometryGenerator.h"
#include "FrameResource.h"
#include "ShadowRenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

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
	Debug,
	Count
};

class ShadowMapApp : public D3DApp
{
public:
	ShadowMapApp(HINSTANCE hInstance);
	ShadowMapApp(const ShadowMapApp& rhs) = delete;
	ShadowMapApp& operator=(const ShadowMapApp& rhs) = delete;
	~ShadowMapApp();

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
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);

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
	void DrawSceneToShadowMap();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuSrv(int index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuSrv(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuDsv(int index) const;

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

	// 天空盒纹理、阴影贴图纹理的在描述符堆中的索引.
	UINT mSkyTexHeapIndex = 0;
	UINT mShadowMapHeapIndex = 0;

	// 这里讲立方体纹理和阴影贴图使用了同一个根描述符表来存储.
	UINT mNullCubeSrvIndex = 0;
	UINT mNullShadowSrvIndex = 0;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mNullGpuSrv;

	PassConstants mMainPassCB;
	PassConstants mShadowPassCB;

	Camera mCamera;

	std::unique_ptr<ShadowRenderTarget> mShadowMap;

	// 用于确定光照产生阴影的范围.
	DirectX::BoundingSphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = 
	{
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f),
		XMFLOAT3(0.0f, -0.707f, -0.707f)
	};
	XMFLOAT3 mRotatedLightDirections[3];

	POINT mLastMousePos;
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShadowMapApp theApp(hInstance);
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