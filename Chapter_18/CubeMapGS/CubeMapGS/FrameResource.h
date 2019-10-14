#pragma once

#include "..\..\..\Common\d3dUtil.h"
#include "..\..\..\Common\MathHelper.h"
#include "..\..\..\Common\UploadBuffer.h"

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	UINT MaterialIndex = 0;
};

/// 将所有绘制过程集合到一起, 需要重构相关的填充方式.
struct PassConstants
{
	DirectX::XMFLOAT4X4 View[7];
	DirectX::XMFLOAT4X4 InvView[7];
	DirectX::XMFLOAT4X4 Proj[7];
	DirectX::XMFLOAT4X4 InvProj[7];
	DirectX::XMFLOAT4X4 ViewProj[7];
	DirectX::XMFLOAT4X4 InvViewProj[7];

	DirectX::XMFLOAT4 EyePosW[7];
	DirectX::XMFLOAT4 RenderTargetSize[7];
	DirectX::XMFLOAT4 InvRenderTargetSize[7];

	DirectX::XMFLOAT4 NearZ[7];
	DirectX::XMFLOAT4 FarZ[7];

	float TotalTime;
	float DeltaTime;
	float pad0;
	float pad1;

	DirectX::XMFLOAT4 AmbientLight;
	Light Lights[MaxLights];
};

struct MaterialData
{
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;

	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource() = default;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdAlloc = nullptr;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialData>> MaterialBuffer = nullptr;

	UINT64 Fence = 0;
};
