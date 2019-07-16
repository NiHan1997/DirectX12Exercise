#pragma once

#include "..\..\..\Common\d3dApp.h"
#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT4 Color;
};

struct ObjectConstants
{
	XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

class PrimitiveApp : public D3DApp
{
public:
	PrimitiveApp(HINSTANCE hInstantce);
	PrimitiveApp(const PrimitiveApp& rhs) = delete;
	PrimitiveApp& operator=(const PrimitiveApp& rhs) = delete;
	~PrimitiveApp();

	virtual bool Initialize() override;

private:
	virtual void OnResize() override;
	virtual void Update(const GameTimer& gt) override;
	virtual void Draw(const GameTimer& gt) override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildConstantsBuffers();
	void BuildGeos();
	void BuildShadersAndInputLayout();
	void BuildPSOs();

private:
	ComPtr<ID3D12RootSignature> mRootSognature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;	

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mGeos;

	ComPtr<ID3DBlob> mvsByteCode = nullptr;
	ComPtr<ID3DBlob> mpsByteCode = nullptr;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

	POINT mLastMousePos;
};
