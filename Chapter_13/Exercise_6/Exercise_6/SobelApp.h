#pragma once
#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"
#include "..\..\..\Common\GeometryGenerator.h"
#include "Waves.h"
#include "FrameResource.h"
#include "SobelFilter.h"
#include "RenderTarget.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct RenderItem
{
	RenderItem() = default;
	~RenderItem() = default;

	int NumFramesDirty = gNumFrameResources;

	int ObjectCBIndex = -1;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	MeshGeometry* Geo = nullptr;
	Material* Mat = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	AlphaTest,
	Transparent,
	Count
};

class SobelApp : public D3DApp
{
public:
	SobelApp(HINSTANCE hInstance);
	SobelApp(const SobelApp& rhs) = delete;
	SobelApp& operator=(const SobelApp& rhs) = delete;
	~SobelApp();

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
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWavesVB(const GameTimer& gt);

	void LoadTextures();
	void BuildRootSignature();
	void BuildPostProcessRootSignature();
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildPSOs();

	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
	void DrawSceneToOffScreenTexture(ID3D12GraphicsCommandList* cmdList);
	void DrawFullscreenQuad(ID3D12GraphicsCommandList* cmdList);
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	float GetHillsHeight(float x, float z) const;
	XMFLOAT3 GetHillsNormal(float x, float z) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuCbvSrvUav(int index) const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGpuCbvSrvUav(int index) const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCpuRtv(int index) const;

private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mWavesRootSignature = nullptr;
	ComPtr<ID3D12RootSignature> mPostProcessRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvSrvUavDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	RenderItem* mWavesRitem = nullptr;

	std::unique_ptr<Waves> mWaves = nullptr;
	std::unique_ptr<RenderTarget> mOffscreenRT = nullptr;
	std::unique_ptr<SobelFilter> mSobelFilter = nullptr;

	PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.1f;
	float mRadius = 50.0f;

	POINT mLastMousePos;
};
