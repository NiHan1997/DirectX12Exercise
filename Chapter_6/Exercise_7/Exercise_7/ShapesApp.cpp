#include "ShapesApp.h"

ShapesApp::ShapesApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
}

bool ShapesApp::Initialize()
{
	if (D3DApp::Initialize() == false)
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildShadersAndInputLayout();
	BuildShapesGeo();
	BuildPSO();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void ShapesApp::Update(const GameTimer& gt)
{
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	XMVECTOR eyePos = XMVectorSet(x, y, z, 1.0f);
	XMVECTOR targetPos = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eyePos, targetPos, up);
	XMStoreFloat4x4(&mView, view);

	XMMATRIX world = XMLoadFloat4x4(&mWorld);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objConstants;
	objConstants.Time = mTimer.TotalTime();
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));

	mObjectCB->CopyData(0, objConstants);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSO.Get()));

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descHeaps), descHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	mCommandList->IASetVertexBuffers(0, 1, &mShapesGeo->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mShapesGeo->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawIndexedInstanced(mShapesGeo->DrawArgs["box"].IndexCount, 1,
		mShapesGeo->DrawArgs["box"].StartIndexLocation,
		mShapesGeo->DrawArgs["box"].BaseVertexLocation, 0);

	mCommandList->DrawIndexedInstanced(mShapesGeo->DrawArgs["pyramid"].IndexCount, 1,
		mShapesGeo->DrawArgs["pyramid"].StartIndexLocation,
		mShapesGeo->DrawArgs["pyramid"].BaseVertexLocation, 0);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();

}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mTheta -= dx;
		mPhi -= dy;

		mPhi = MathHelper::Clamp(mPhi, 0.1f, XM_PI - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		mRadius += dx - dy;

		mRadius = MathHelper::Clamp(mRadius, 3.0f, 20.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[1];
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serilizedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serilizedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serilizedRootSig->GetBufferPointer(),
		serilizedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void ShapesApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(mCbvHeap.GetAddressOf())));
}

void ShapesApp::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
	const UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objCBAddr = mObjectCB->Resource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = objCBAddr;
	cbvDesc.SizeInBytes = objCBByteSize;

	md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void ShapesApp::BuildShadersAndInputLayout()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_1");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void ShapesApp::BuildShapesGeo()
{
	std::array<Vertex, 13> vertices =
	{
		// Box.
		Vertex({XMFLOAT3(-2.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(-2.0f, 1.0f, -1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(0.0f, 1.0f, -1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(0.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(-2.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Brown)}),
		Vertex({XMFLOAT3(-2.0f, 1.0f, 1.0f), XMFLOAT4(Colors::HotPink)}),
		Vertex({XMFLOAT3(0.0f, 1.0f, 1.0f), XMFLOAT4(Colors::Purple)}),
		Vertex({XMFLOAT3(0.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Silver)}),

		// Pyramid.
		Vertex({XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Red)}),
		Vertex({XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Blue)}),
		Vertex({XMFLOAT3(3.0f, -1.0f, 1.0f), XMFLOAT4(Colors::Yellow)}),
		Vertex({XMFLOAT3(3.0f, -1.0f, -1.0f), XMFLOAT4(Colors::Green)}),
		Vertex({XMFLOAT3(2.0f, 1.4f, 0.0f), XMFLOAT4(Colors::Brown)})
	};

	std::array<std::uint16_t, 54> indices =
	{
		// 立方体.
		0, 1, 2, 0, 2, 3,
		4, 6, 5, 4, 7, 6,
		4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7,
		1, 5, 6, 1, 6, 2,
		4, 0, 2, 4, 3, 7,

		// 金字塔.
		0, 2, 1, 0, 3, 2,
		0, 1, 4, 0, 4, 3,
		2, 3, 4, 1, 2, 4
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mShapesGeo = std::make_unique<MeshGeometry>();
	mShapesGeo->Name = "shapes";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, mShapesGeo->VertexBufferCPU.GetAddressOf()));
	CopyMemory(mShapesGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, mShapesGeo->IndexBufferCPU.GetAddressOf()));
	CopyMemory(mShapesGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mShapesGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, mShapesGeo->VertexBufferUploader);

	mShapesGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, mShapesGeo->IndexBufferUploader);

	mShapesGeo->VertexBufferByteSize = vbByteSize;
	mShapesGeo->VertexByteStride = sizeof(Vertex);
	mShapesGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mShapesGeo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = 36;
	boxSubmesh.StartIndexLocation = 0;
	boxSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry pyramidSubmesh;
	pyramidSubmesh.IndexCount = 18;
	pyramidSubmesh.StartIndexLocation = 36;
	pyramidSubmesh.BaseVertexLocation = 8;

	mShapesGeo->DrawArgs["box"] = boxSubmesh;
	mShapesGeo->DrawArgs["pyramid"] = pyramidSubmesh;
}

void ShapesApp::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&psoDesc, 
		IID_PPV_ARGS(mPSO.GetAddressOf())));
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif

	try
	{
		ShapesApp theApp(hInstance);
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
