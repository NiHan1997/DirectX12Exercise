#include "PrimitiveApp.h"

PrimitiveApp::PrimitiveApp(HINSTANCE hInstantce) : D3DApp(hInstantce)
{
}

PrimitiveApp::~PrimitiveApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool PrimitiveApp::Initialize()
{
	if (D3DApp::Initialize() == false)
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildConstantsBuffers();
	BuildGeos();
	BuildShadersAndInputLayout();
	BuildPSOs();

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
	FlushCommandQueue();

	return true;
}

void PrimitiveApp::OnResize()
{
	D3DApp::OnResize();

	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, proj);
}

void PrimitiveApp::Update(const GameTimer& gt)
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
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));

	mObjectCB->CopyData(0, objConstants);
}

void PrimitiveApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["point"].Get()));

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

	mCommandList->SetGraphicsRootSignature(mRootSognature.Get());

	mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

	// IA阶段, 首先绘制点列表.
	mCommandList->IASetVertexBuffers(0, 1, &mGeos->VertexBufferView());
	mCommandList->IASetIndexBuffer(&mGeos->IndexBufferView());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

	mCommandList->DrawIndexedInstanced(mGeos->DrawArgs["pointList"].IndexCount, 1,
		mGeos->DrawArgs["pointList"].StartIndexLocation,
		mGeos->DrawArgs["pointList"].BaseVertexLocation, 0);

	// 绘制线条带.
	mCommandList->SetPipelineState(mPSOs["line"].Get());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
	mCommandList->DrawIndexedInstanced(mGeos->DrawArgs["lineStrip"].IndexCount, 1,
		mGeos->DrawArgs["lineStrip"].StartIndexLocation,
		mGeos->DrawArgs["lineStrip"].BaseVertexLocation, 0);

	// 胡子线列表.
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	mCommandList->DrawIndexedInstanced(mGeos->DrawArgs["lineList"].IndexCount, 1,
		mGeos->DrawArgs["lineList"].StartIndexLocation,
		mGeos->DrawArgs["lineList"].BaseVertexLocation, 0);

	// 绘制三角形带.
	mCommandList->SetPipelineState(mPSOs["triangle"].Get());
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	mCommandList->DrawIndexedInstanced(mGeos->DrawArgs["triangleStrip"].IndexCount, 1,
		mGeos->DrawArgs["triangleStrip"].StartIndexLocation,
		mGeos->DrawArgs["triangleStrip"].BaseVertexLocation, 0);

	//绘制三角形列表.
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mCommandList->DrawIndexedInstanced(mGeos->DrawArgs["triangleList"].IndexCount, 1,
		mGeos->DrawArgs["triangleList"].StartIndexLocation,
		mGeos->DrawArgs["triangleList"].BaseVertexLocation, 0);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void PrimitiveApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void PrimitiveApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void PrimitiveApp::OnMouseMove(WPARAM btnState, int x, int y)
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

		mRadius = MathHelper::Clamp(mRadius, 3.0f, 30.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void PrimitiveApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE cbvTable;
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParamter[1];
	slotRootParamter[0].InitAsDescriptorTable(1, &cbvTable);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParamter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serilizedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serilizedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serilizedRootSig->GetBufferPointer(),
		serilizedRootSig->GetBufferSize(), IID_PPV_ARGS(mRootSognature.GetAddressOf())));
}

void PrimitiveApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NodeMask = 0;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(mCbvHeap.GetAddressOf())));
}

void PrimitiveApp::BuildConstantsBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(md3dDevice.Get(), 1, true);
	const UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objCBAddr = mObjectCB->Resource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = objCBAddr;
	cbvDesc.SizeInBytes = objCBByteSize;

	md3dDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void PrimitiveApp::BuildGeos()
{
	std::vector<Vertex> vertices =
	{
		// 点列表.
		Vertex({ XMFLOAT3(-4.0f, -4.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(-3.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(-2.0f, -3.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(0.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(1.0f, -2.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(3.0f,  0.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(5.0f, -2.0f, -2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(7.0f,  1.0f, -2.0f), XMFLOAT4(Colors::Red) }),

		// 线条带.
		Vertex({ XMFLOAT3(-4.0f, -4.0f, -1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-3.0f,  0.0f, -1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(-2.0f, -3.0f, -1.0f), XMFLOAT4(Colors::HotPink) }),
		Vertex({ XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(1.0f, -2.0f, -1.0f), XMFLOAT4(Colors::Purple) }),
		Vertex({ XMFLOAT3(3.0f,  0.0f, -1.0f), XMFLOAT4(Colors::Brown) }),
		Vertex({ XMFLOAT3(5.0f, -2.0f, -1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(7.0f,  1.0f, -1.0f), XMFLOAT4(Colors::Black) }),

		// 线列表.
		Vertex({ XMFLOAT3(-4.0f, -4.0f, 0.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(-3.0f,  0.0f, 0.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(-2.0f, -3.0f, 0.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(0.0f,  0.0f, 0.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(1.0f, -2.0f, 0.0f), XMFLOAT4(Colors::Purple) }),
		Vertex({ XMFLOAT3(3.0f,  0.0f, 0.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(5.0f, -2.0f, 0.0f), XMFLOAT4(Colors::HotPink) }),
		Vertex({ XMFLOAT3(7.0f,  1.0f, 0.0f), XMFLOAT4(Colors::Brown) }),

		// 三角形带.
		Vertex({ XMFLOAT3(-4.0f, -4.0f, 1.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-3.0f,  0.0f, 1.0f), XMFLOAT4(Colors::Black) }),
		Vertex({ XMFLOAT3(-2.0f, -3.0f, 1.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(0.0f,   0.0f, 1.0f), XMFLOAT4(Colors::Green) }),
		Vertex({ XMFLOAT3(1.0f,  -2.0f, 1.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(3.0f,   0.0f, 1.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(5.0f,  -2.0f, 1.0f), XMFLOAT4(Colors::Cyan) }),
		Vertex({ XMFLOAT3(7.0f,   1.0f, 1.0f), XMFLOAT4(Colors::Magenta) }),

		// 三角形列表.
		Vertex({ XMFLOAT3(-4.0f, -4.0f, 2.0f), XMFLOAT4(Colors::White) }),
		Vertex({ XMFLOAT3(-3.0f,  0.0f, 2.0f), XMFLOAT4(Colors::Yellow) }),
		Vertex({ XMFLOAT3(-2.0f, -3.0f, 2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(0.0f,   0.0f, 2.0f), XMFLOAT4(Colors::Purple) }),
		Vertex({ XMFLOAT3(3.0f,   0.0f, 2.0f), XMFLOAT4(Colors::Red) }),
		Vertex({ XMFLOAT3(1.0f,  -2.0f, 2.0f), XMFLOAT4(Colors::Gray) }),
		Vertex({ XMFLOAT3(5.0f,  -2.0f, 2.0f), XMFLOAT4(Colors::HotPink) }),
		Vertex({ XMFLOAT3(7.0f,   1.0f, 2.0f), XMFLOAT4(Colors::Blue) }),
		Vertex({ XMFLOAT3(8.0f,   0.0f, 2.0f), XMFLOAT4(Colors::Honeydew) })
	};

	std::vector<std::uint16_t> indices =
	{
		// 点列表.
		0, 1, 2, 3, 4, 5, 6, 7,

		// 线条带.
		0, 1, 2, 3, 4, 5, 6, 7,

		// 线列表.
		0, 1, 2, 3, 4, 5, 6, 7,

		// 三角形带.
		0, 1, 2, 1, 2, 3,
		2, 3, 4, 3, 4, 5,
		4, 5, 6, 5, 6, 7,

		// 三角形列表.
		0, 1, 2, 3, 4, 5, 6, 7, 8
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mGeos = std::make_unique<MeshGeometry>();
	mGeos->Name = "primitiveType";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, mGeos->VertexBufferCPU.GetAddressOf()));
	CopyMemory(mGeos->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, mGeos->IndexBufferCPU.GetAddressOf()));
	CopyMemory(mGeos->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	mGeos->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, mGeos->VertexBufferUploader);

	mGeos->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, mGeos->IndexBufferUploader);

	mGeos->VertexBufferByteSize = vbByteSize;
	mGeos->VertexByteStride = sizeof(Vertex);
	mGeos->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeos->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry pointList;
	pointList.IndexCount = 8;
	pointList.StartIndexLocation = 0;
	pointList.BaseVertexLocation = 0;

	SubmeshGeometry lineStrip;
	lineStrip.IndexCount = 8;
	lineStrip.StartIndexLocation = 8;
	lineStrip.BaseVertexLocation = 8;

	SubmeshGeometry lineList;
	lineList.IndexCount = 8;
	lineList.StartIndexLocation = 16;
	lineList.BaseVertexLocation = 16;

	SubmeshGeometry triangleStrip;
	triangleStrip.IndexCount = 18;
	triangleStrip.StartIndexLocation = 24;
	triangleStrip.BaseVertexLocation = 24;

	SubmeshGeometry triangleList;
	triangleList.IndexCount = 9;
	triangleList.StartIndexLocation = 42;
	triangleList.BaseVertexLocation = 32;

	mGeos->DrawArgs["pointList"] = pointList;
	mGeos->DrawArgs["lineStrip"] = lineStrip;
	mGeos->DrawArgs["lineList"] = lineList;
	mGeos->DrawArgs["triangleStrip"] = triangleStrip;
	mGeos->DrawArgs["triangleList"] = triangleList;
}

void PrimitiveApp::BuildShadersAndInputLayout()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "VS", "vs_5_1");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders/color.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void PrimitiveApp::BuildPSOs()
{
	// 绘制点的PSO.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pointPsoDesc;
	ZeroMemory(&pointPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	pointPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	pointPsoDesc.pRootSignature = mRootSognature.Get();
	pointPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()),
		mvsByteCode->GetBufferSize()
	};
	pointPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()),
		mpsByteCode->GetBufferSize()
	};
	pointPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	pointPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pointPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	pointPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	pointPsoDesc.NumRenderTargets = 1;
	pointPsoDesc.RTVFormats[0] = mBackBufferFormat;
	pointPsoDesc.SampleMask = UINT_MAX;
	pointPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	pointPsoDesc.SampleDesc.Count = 1;
	pointPsoDesc.SampleDesc.Quality = 0;
	pointPsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&pointPsoDesc,
		IID_PPV_ARGS(mPSOs["point"].GetAddressOf())));

	// 绘制线的PSO.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC linePsoDesc = pointPsoDesc;
	linePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&linePsoDesc,
		IID_PPV_ARGS(mPSOs["line"].GetAddressOf())));

	// 绘制三角形的PSO.
	D3D12_GRAPHICS_PIPELINE_STATE_DESC trianglePsoDesc = pointPsoDesc;
	trianglePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&trianglePsoDesc,
		IID_PPV_ARGS(mPSOs["triangle"].GetAddressOf())));
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_ALLOC_MEM_DF);
#endif

	try
	{
		PrimitiveApp theApp(hInstance);
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
