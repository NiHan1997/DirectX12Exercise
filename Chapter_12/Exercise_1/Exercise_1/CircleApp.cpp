#include "CircleApp.h"

CircleApp::CircleApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

CircleApp::~CircleApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool CircleApp::Initialize()
{
	if (D3DApp::Initialize() == false)
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayouts();
	BuildCircleGeometry();
	BuildRenderItem();
	BuildFrameResources();
	BuildPSO();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	return true;
}

void CircleApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void CircleApp::Update(const GameTimer& gt)
{
	UpdateCamera(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void CircleApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->SetGraphicsRootConstantBufferView(0, mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress());
	mCommandList->SetGraphicsRootConstantBufferView(1, mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress());

	mCommandList->IASetVertexBuffers(0, 1, &mCircleGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mCircleGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(mCircleRitem->PrimitiveType);

	mCommandList->DrawIndexedInstanced(mCircleRitem->IndexCount, 1,
		mCircleRitem->StartIndexLocation, mCircleRitem->BaseVertexLocation, 0);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrentFence;
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void CircleApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void CircleApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void CircleApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta -= dx;
		mPhi -= dy;

		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.2f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.2f * static_cast<float>(y - mLastMousePos.y);

		mRadius += dx - dy;

		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void CircleApp::UpdateCamera(const GameTimer& gt)
{
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void CircleApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjCB = mCurrFrameResource->ObjectCB.get();
	if (mCircleRitem->NumFramesDirty > 0)
	{
		XMMATRIX world = XMLoadFloat4x4(&mCircleRitem->World);
		XMMATRIX texTransform = XMLoadFloat4x4(&mCircleRitem->TexTransform);

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

		currObjCB->CopyData(mCircleRitem->ObjCBIndex, objConstants);
		mCircleRitem->NumFramesDirty--;
	}
}

void CircleApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void CircleApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsConstantBufferView(0, 0);		// ObjCB.
	slotRootParameter[1].InitAsConstantBufferView(1, 0);		// PassCB.

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serilizedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serilizedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob != nullptr) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serilizedRootSig->GetBufferPointer(),
		serilizedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void CircleApp::BuildShadersAndInputLayouts()
{
	mShaders["VS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["GS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["PS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void CircleApp::BuildCircleGeometry()
{
	// 细分次数和圆形半径.
	int subdivision = 20;
	int radius = 5;

	// 顶点数据和索引数据一起对应.
	std::vector<Vertex> vertices(subdivision + 1);
	std::vector<std::uint16_t> indices(subdivision + 1);

	for (int i = 0; i <= subdivision; ++i)
	{
		float x = 5 * cosf(i * XM_2PI / subdivision);
		float z = 5 * sinf(i * XM_2PI / subdivision);

		vertices[i].Position = { x, 0, z };
		vertices[i].Color = XMFLOAT4(Colors::Red);

		indices[i] = i;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mCircleGeo = std::make_unique<MeshGeometry>();
	mCircleGeo->Name = "circleGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, mCircleGeo->VertexBufferCPU.GetAddressOf()));
	CopyMemory(mCircleGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, mCircleGeo->IndexBufferCPU.GetAddressOf()));
	CopyMemory(mCircleGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mCircleGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, mCircleGeo->VertexBufferUploader);

	mCircleGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, mCircleGeo->IndexBufferUploader);

	mCircleGeo->VertexByteStride = sizeof(Vertex);
	mCircleGeo->VertexBufferByteSize = vbByteSize;
	mCircleGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mCircleGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	mCircleGeo->DrawArgs["circle"] = submesh;
}

void CircleApp::BuildRenderItem()
{
	mCircleRitem = std::make_unique<RenderItem>();
	mCircleRitem->World = MathHelper::Identity4x4();
	mCircleRitem->TexTransform = MathHelper::Identity4x4();
	mCircleRitem->ObjCBIndex = 0;
	mCircleRitem->Geo = mCircleGeo.get();
	mCircleRitem->Mat = nullptr;
	mCircleRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
	mCircleRitem->IndexCount = mCircleRitem->Geo->DrawArgs["circle"].IndexCount;
	mCircleRitem->StartIndexLocation = mCircleRitem->Geo->DrawArgs["circle"].StartIndexLocation;
	mCircleRitem->BaseVertexLocation = mCircleRitem->Geo->DrawArgs["circle"].BaseVertexLocation;
}

void CircleApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 1, 1));
	}
}

void CircleApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueLinePsoDesc;
	ZeroMemory(&opaqueLinePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaqueLinePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaqueLinePsoDesc.pRootSignature = mRootSignature.Get();
	opaqueLinePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["VS"]->GetBufferPointer()),
		mShaders["VS"]->GetBufferSize()
	};
	opaqueLinePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["GS"]->GetBufferPointer()),
		mShaders["GS"]->GetBufferSize()
	};
	opaqueLinePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["PS"]->GetBufferPointer()),
		mShaders["PS"]->GetBufferSize()
	};
	opaqueLinePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaqueLinePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaqueLinePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaqueLinePsoDesc.SampleMask = UINT_MAX;
	opaqueLinePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	opaqueLinePsoDesc.NumRenderTargets = 1;
	opaqueLinePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaqueLinePsoDesc.SampleDesc.Count = 1;
	opaqueLinePsoDesc.SampleDesc.Quality = 0;
	opaqueLinePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueLinePsoDesc, 
		IID_PPV_ARGS(mPSO.GetAddressOf())));
}
