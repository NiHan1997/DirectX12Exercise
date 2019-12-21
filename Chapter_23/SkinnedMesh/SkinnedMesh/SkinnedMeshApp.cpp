#include "SkinnedMeshApp.h"

SkinnedMeshApp::SkinnedMeshApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
	mSceneBounds.Center = { 0.0f, 0.0f, 0.0f };
	mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

SkinnedMeshApp::~SkinnedMeshApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool SkinnedMeshApp::Initialize()
{
	if (D3DApp::Initialize() == false)
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	mShadowMap = std::make_unique<ShadowRenderTarget>(md3dDevice.Get(), 2048, 2048);
	mSSAO = std::make_unique<SSAO>(md3dDevice.Get(), mCommandList.Get(), mClientWidth, mClientHeight);

	LoadSkinnedModel();
	LoadTextures();
	BuildRootSignature();
	BuildSSAORootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapesGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	mSSAO->SetPSOs(mPSOs["ssao"].Get(), mPSOs["ssaoHorzBlur"].Get(), mPSOs["ssaoVertBlur"].Get());
	mSSAO->SetComputeRootSig(mSSAORootSignature.Get());

	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();

	return true;
}

void SkinnedMeshApp::CreateRtvAndDsvDescriptorHeaps()
{
	// SwapChain(2) + Normal(1) + Ambient(2).
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NodeMask = 0;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 3;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc,
		IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

	// CurrentDepth(1) + ShadowMap(1).
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NodeMask = 0;
	dsvHeapDesc.NumDescriptors = 2;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc,
		IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void SkinnedMeshApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(XM_PIDIV4, AspectRatio(), 1.0f, 1000.0f);

	if (mSSAO != nullptr)
	{
		mSSAO->OnResize(mClientWidth, mClientHeight);
		mSSAO->RebuildDescriptors(mDepthStencilBuffer.Get());
	}
}

void SkinnedMeshApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	UpdateLightDir(gt);

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateSkinnedCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);
	UpdateSsaoCB(gt);
}

void SkinnedMeshApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;
	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["shadow"].Get()));

	ID3D12DescriptorHeap* descHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descHeaps), descHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	///
	/// ������Ӱ��ͼ.
	///
	auto matBufferAddr = mCurrFrameResource->MaterialBuffer->Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootShaderResourceView(3, matBufferAddr);

	mCommandList->SetGraphicsRootDescriptorTable(5, GetGpuSrv(0));

	DrawSceneToShadowMap();

	///
	/// �������/��������.
	///
	DrawNormalAndDepthMap();

	///
	/// ����SSAO.
	///
	mCommandList->SetGraphicsRootSignature(mSSAORootSignature.Get());
	mSSAO->ComputeSSAO(mCommandList.Get(), mCurrFrameResource, 3);

	///
	/// ����Ⱦ����.
	///
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// ����ֻ��Ҫ�����ȾĿ�꼴��, ��Ϊ���ֵ�ڻ������/��������ʱ�Ѿ���ȡ, �Ҳ���Ҫ�ı�.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCBAddr = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddr);
	mCommandList->SetGraphicsRootShaderResourceView(3, matBufferAddr);
	mCommandList->SetGraphicsRootDescriptorTable(4, GetGpuSrv(mSkyTexHeapIndex));
	mCommandList->SetGraphicsRootDescriptorTable(5, GetGpuSrv(0));

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedOpaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Debug]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

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

void SkinnedMeshApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void SkinnedMeshApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void SkinnedMeshApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void SkinnedMeshApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

void SkinnedMeshApp::AnimateMaterials(const GameTimer& gt)
{
}

void SkinnedMeshApp::UpdateLightDir(const GameTimer& gt)
{
	///
	/// ��ת��Դ.
	///
	mLightRotationAngle += 0.1f * gt.DeltaTime();
	XMMATRIX rotationY = XMMatrixRotationY(mLightRotationAngle);

	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, rotationY);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}
}

void SkinnedMeshApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjCB->CopyData(e->ObjectCBIndex, objConstants);
			e->NumFramesDirty--;
		}
	}
}

void SkinnedMeshApp::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto skinnedCB = mCurrFrameResource->SkinnedCB.get();
	mSkinnedModelInst->UpdateSkinnedAnimation(gt.DeltaTime());

	SkinnedConstants skinnedConstants;
	std::copy(
		std::begin(mSkinnedModelInst->FinalTransforms),
		std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);

	skinnedCB->CopyData(0, skinnedConstants);
}

void SkinnedMeshApp::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMatBuffer = mCurrFrameResource->MaterialBuffer.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(matTransform));

			currMatBuffer->CopyData(mat->MatCBIndex, matData);
			mat->NumFramesDirty--;
		}
	}
}

void SkinnedMeshApp::UpdateShadowTransform(const GameTimer& gt)
{
	// ֻ������ԴͶ����Ӱ.
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMStoreFloat3(&mLightPosW, lightPos);

	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, up);

	// ����Χ��任����Դ�ռ�.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// ����͸��ͶӰ����.
	float left = sphereCenterLS.x - mSceneBounds.Radius;
	float right = sphereCenterLS.x + mSceneBounds.Radius;
	float top = sphereCenterLS.y + mSceneBounds.Radius;
	float bottom = sphereCenterLS.y - mSceneBounds.Radius;
	float nearZ = sphereCenterLS.z - mSceneBounds.Radius;
	float farZ = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = nearZ;
	mLightFarZ = farZ;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);

	// ͶӰ�ռ� -> ����ռ�.
	XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX shadowTransform = lightView * lightProj * tex;
	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, shadowTransform);
}

void SkinnedMeshApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, tex);
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

	mMainPassCB.AmbientLight = { 0.8f, 0.8f, 0.8f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.4f, 0.4f, 0.5f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.1f, 0.1f, 0.1f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.0f, 0.0f, 0.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void SkinnedMeshApp::UpdateShadowPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mShadowPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mShadowPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mShadowPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mShadowPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mShadowPassCB.InvViewProj, XMMatrixTranspose(invViewProj));

	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(1, mShadowPassCB);
}

void SkinnedMeshApp::UpdateSsaoCB(const GameTimer& gt)
{
	SSAOConstants ssaoCB;

	XMMATRIX proj = mCamera.GetProj();
	XMMATRIX tex(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;
	XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(proj * tex));

	mSSAO->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSSAO->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
	ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
	ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

	ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / mSSAO->SSAOMapWidth(), 1.0f / mSSAO->SSAOMapHeight());

	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSsaoCB = mCurrFrameResource->SSAOCB.get();
	currSsaoCB->CopyData(0, ssaoCB);
}

void SkinnedMeshApp::LoadSkinnedModel()
{
	std::vector<M3DLoader::SkinnedVertex> vertices;
	std::vector<std::uint16_t> indices;

	M3DLoader m3dLoader;
	m3dLoader.LoadM3d(mSkinnedModelFilename, vertices, indices,
		mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = "Take1";
	mSkinnedModelInst->TimePos = 0.0f;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = mSkinnedModelFilename;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, geo->VertexBufferCPU.GetAddressOf()));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, geo->IndexBufferCPU.GetAddressOf()));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (UINT i = 0; i < (UINT)mSkinnedSubsets.size(); ++i)
	{
		SubmeshGeometry submesh;
		std::string name = "sm_" + std::to_string(i);

		submesh.IndexCount = (UINT)mSkinnedSubsets[i].FaceCount * 3;
		submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs[name] = submesh;
	}

	mGeometries[geo->Name] = std::move(geo);
}

void SkinnedMeshApp::LoadTextures()
{
	std::vector<std::string> texNames =
	{
		"bricksDiffuseMap",
		"bricksNormalMap",
		"tileDiffuseMap",
		"tileNormalMap",
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap"
	};

	std::vector<std::wstring> texFilenames =
	{
		L"../../../Textures/bricks.dds",
		L"../../../Textures/bricks_nmap.dds",
		L"../../../Textures/tile.dds",
		L"../../../Textures/tile_nmap.dds",
		L"../../../Textures/white1x1.dds",
		L"../../../Textures/default_nmap.dds",
		L"../../../Textures/desertcube1024.dds"
	};

	// ���ؽ�ɫ��Ҫ������.
	for (UINT i = 0; i < (UINT)mSkinnedMats.size(); ++i)
	{
		std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
		std::string normalName = mSkinnedMats[i].NormalMapName;

		std::wstring diffuseFilename = L"../../../Textures/" + AnsiToWString(diffuseName);
		std::wstring normalFilename = L"../../../Textures/" + AnsiToWString(normalName);

		diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));
		normalName = normalName.substr(0, normalName.find_last_of("."));

		mSkinnedTextureNames.push_back(diffuseName);
		texNames.push_back(diffuseName);
		texFilenames.push_back(diffuseFilename);

		mSkinnedTextureNames.push_back(normalName);
		texNames.push_back(normalName);
		texFilenames.push_back(normalFilename);
	}

	// ��������.
	for (int i = 0; i < (int)texFilenames.size(); ++i)
	{
		if (mTextures.find(texNames[i]) == std::end(mTextures))
		{
			auto texMap = std::make_unique<Texture>();
			texMap->Name = texNames[i];
			texMap->Filename = texFilenames[i];

			ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(),
				texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));
			mTextures[texMap->Name] = std::move(texMap);
		}
	}
}

void SkinnedMeshApp::BuildRootSignature()
{
	// CubeMap & ShadowMap & SSAOMap.
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 1, 0);

	// Diffuse & Normal.
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 4, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[6];
	slotRootParameter[0].InitAsConstantBufferView(0, 0);		// ObjCB.
	slotRootParameter[1].InitAsConstantBufferView(1, 0);		// SkinnedCB.
	slotRootParameter[2].InitAsConstantBufferView(2, 0);		// PassCB.
	slotRootParameter[3].InitAsShaderResourceView(0, 0);		// MatBuffer.
	slotRootParameter[4].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[5].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto staticSamplers = GetStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(6, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
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

void SkinnedMeshApp::BuildSSAORootSignature()
{
	// Normal & Depth.
	CD3DX12_DESCRIPTOR_RANGE texTable0;
	texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	// RandomVector or SSAOMapSrv.
	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

	// SSAOMap Uav.
	CD3DX12_DESCRIPTOR_RANGE texTable2;
	texTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[4];
	slotRootParameter[0].InitAsConstantBufferView(0, 0);		// SSAOCB.
	slotRootParameter[1].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_ALL);
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable2, D3D12_SHADER_VISIBILITY_ALL);

	auto staticSamplers = GetStaticSamplers();
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serilizedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serilizedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
	if (errorBlob != nullptr) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serilizedRootSig->GetBufferPointer(),
		serilizedRootSig->GetBufferSize(), IID_PPV_ARGS(mSSAORootSignature.GetAddressOf())));
}

void SkinnedMeshApp::BuildDescriptorHeaps()
{
	// Basic : Tex(7 + 10)
	// Shadow : Shadow(1)
	// SSAO : Normal(1) + Depth(1) + RandomVector(1) + SSAOSrv(2) + SSAOUav(2)
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NodeMask = 0;
	srvHeapDesc.NumDescriptors = 25;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc,
		IID_PPV_ARGS(mSrvDescriptorHeap.GetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	///
	/// Texture & Cube.
	///
	std::vector<ComPtr<ID3D12Resource>> tex2DList =
	{
		mTextures["bricksDiffuseMap"]->Resource,
		mTextures["bricksNormalMap"]->Resource,
		mTextures["tileDiffuseMap"]->Resource,
		mTextures["tileNormalMap"]->Resource,
		mTextures["defaultDiffuseMap"]->Resource,
		mTextures["defaultNormalMap"]->Resource
	};

	mSkinnedSrvHeapStartIndex = (UINT)tex2DList.size();
	for (UINT i = 0; i < (UINT)mSkinnedTextureNames.size(); ++i)
	{
		auto texResource = mTextures[mSkinnedTextureNames[i]]->Resource;
		assert(texResource != nullptr);
		tex2DList.push_back(texResource);
	}

	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (int i = 0; i < (int)tex2DList.size(); ++i)
	{
		srvDesc.Format = tex2DList[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
	}

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = skyCubeMap->GetDesc().Format;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.TextureCube.MipLevels = skyCubeMap->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	///
	/// ShadowMap & NormalMap & DepthMap & RandomVector & SSAOMap.
	///
	mSkyTexHeapIndex = (UINT)tex2DList.size();
	mShadowMapHeapIndex = mSkyTexHeapIndex + 1;
	mSSAOHeapIndexStart = mShadowMapHeapIndex + 1;
	mSSAOAmbientMapIndex = mSSAOHeapIndexStart + 2;

	mShadowMap->BuildDescriptors(
		GetCpuSrv(mShadowMapHeapIndex),
		GetGpuSrv(mShadowMapHeapIndex),
		GetCpuDsv(1));

	mSSAO->BuildDescriptors(
		mDepthStencilBuffer.Get(),
		GetCpuSrv(mSSAOHeapIndexStart),
		GetGpuSrv(mSSAOHeapIndexStart),
		GetCpuRtv(SwapChainBufferCount),
		mCbvSrvUavDescriptorSize,
		mRtvDescriptorSize);
}

void SkinnedMeshApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO skinnedDefines[] =
	{
		"SKINNED", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedVS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders/Default.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowVS"] = d3dUtil::CompileShader(L"Shaders/Shadows.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedShadowVS"] = d3dUtil::CompileShader(L"Shaders/Shadows.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["shadowPS"] = d3dUtil::CompileShader(L"Shaders/Shadows.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders/Debug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders/Debug.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["drawNormalsVS"] = d3dUtil::CompileShader(L"Shaders/DrawNormals.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skinnedDrawNormalsVS"] = d3dUtil::CompileShader(L"Shaders/DrawNormals.hlsl", skinnedDefines, "VS", "vs_5_1");
	mShaders["drawNormalsPS"] = d3dUtil::CompileShader(L"Shaders/DrawNormals.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoVS"] = d3dUtil::CompileShader(L"Shaders/Ssao.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["ssaoPS"] = d3dUtil::CompileShader(L"Shaders/Ssao.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["ssaoHorzBlur"] = d3dUtil::CompileShader(L"Shaders/SsaoBlur.hlsl", nullptr, "HorzBlurCS", "cs_5_1");
	mShaders["ssaoVertBlur"] = d3dUtil::CompileShader(L"Shaders/SsaoBlur.hlsl", nullptr, "VertBlurCS", "cs_5_1");

	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders/Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders/Sky.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	mSkinnedInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHTS", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONEINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT, 0, 56,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void SkinnedMeshApp::BuildShapesGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	///
	/// ���㻺������������������Ӧ.
	///
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size() + boxVertexOffset;
	UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
	UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;
	UINT quadVertexOffset = (UINT)cylinder.Vertices.size() + cylinderVertexOffset;

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size() + boxIndexOffset;
	UINT sphereIndexOffset = (UINT)grid.Indices32.size() + gridIndexOffset;
	UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;
	UINT quadIndexOffset = (UINT)cylinder.Indices32.size() + cylinderIndexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	///
	/// ���㻺�����ϲ�.
	///
	std::vector<Vertex> vertices(box.Vertices.size() + grid.Vertices.size() + sphere.Vertices.size() +
		cylinder.Vertices.size() + quad.Vertices.size());
	UINT k = 0;
	for (UINT i = 0; i < (UINT)box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].Tangent = box.Vertices[i].TangentU;
	}
	for (UINT i = 0; i < (UINT)grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].Tangent = grid.Vertices[i].TangentU;
	}
	for (UINT i = 0; i < (UINT)sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].Tangent = sphere.Vertices[i].TangentU;
	}
	for (UINT i = 0; i < (UINT)cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].Tangent = cylinder.Vertices[i].TangentU;
	}
	for (UINT i = 0; i < (UINT)quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
		vertices[k].Tangent = quad.Vertices[i].TangentU;
	}

	///
	/// �����������ϲ�.
	///
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());
	indices.insert(indices.end(), quad.GetIndices16().begin(), quad.GetIndices16().end());

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, geo->VertexBufferCPU.GetAddressOf()));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, geo->IndexBufferCPU.GetAddressOf()));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(),
		indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void SkinnedMeshApp::BuildMaterials()
{
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->NormalSrvHeapIndex = 1;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	bricks->Roughness = 0.3f;

	auto tile = std::make_unique<Material>();
	tile->Name = "tile";
	tile->MatCBIndex = 1;
	tile->DiffuseSrvHeapIndex = 2;
	tile->NormalSrvHeapIndex = 3;
	tile->DiffuseAlbedo = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);
	tile->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	tile->Roughness = 0.1f;

	auto mirror = std::make_unique<Material>();
	mirror->Name = "mirror";
	mirror->MatCBIndex = 2;
	mirror->DiffuseSrvHeapIndex = 4;
	mirror->NormalSrvHeapIndex = 5;
	mirror->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mirror->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror->Roughness = 0.1f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->MatCBIndex = 3;
	sky->DiffuseSrvHeapIndex = 6;
	sky->NormalSrvHeapIndex = 7;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["tile"] = std::move(tile);
	mMaterials["mirror"] = std::move(mirror);
	mMaterials["sky"] = std::move(sky);

	// ��ɫ��Ҫ�Ĳ���.
	UINT matCBIndex = 4;
	UINT srvHeapIndex = mSkinnedSrvHeapStartIndex;
	for (UINT i = 0; i < (UINT)mSkinnedMats.size(); ++i)
	{
		auto mat = std::make_unique<Material>();
		mat->Name = mSkinnedMats[i].Name;
		mat->MatCBIndex = matCBIndex++;
		mat->DiffuseSrvHeapIndex = srvHeapIndex++;
		mat->NormalSrvHeapIndex = srvHeapIndex++;
		mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
		mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
		mat->Roughness = mSkinnedMats[i].Roughness;

		mMaterials[mat->Name] = std::move(mat);
	}
}

void SkinnedMeshApp::BuildRenderItems()
{
	// ��պ�.
	auto skyRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjectCBIndex = 0;
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	// ���Բ�.
	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->World = MathHelper::Identity4x4();
	quadRitem->TexTransform = MathHelper::Identity4x4();
	quadRitem->ObjectCBIndex = 1;
	quadRitem->Mat = mMaterials["bricks"].get();
	quadRitem->Geo = mGeometries["shapeGeo"].get();
	quadRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));

	// ������.
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 0.5f, 1.0f));
	boxRitem->ObjectCBIndex = 2;
	boxRitem->Mat = mMaterials["bricks"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	// ��������.
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->ObjectCBIndex = 3;
	gridRitem->Mat = mMaterials["tile"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	// ���Ӻ�����.
	XMMATRIX brickTexTransform = XMMatrixScaling(1.5f, 2.0f, 1.0f);
	UINT objCBIndex = 4;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		XMStoreFloat4x4(&leftCylRitem->World, leftCylWorld);
		XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->ObjectCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&rightCylRitem->World, rightCylWorld);
		XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->ObjectCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjectCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["mirror"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjectCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["mirror"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

	// ���ν�ɫ.
	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		std::string submeshName = "sm_" + std::to_string(i);
		auto ritem = std::make_unique<RenderItem>();

		XMMATRIX modelScale = XMMatrixScaling(0.05f, 0.05f, -0.05f);
		XMMATRIX modelRot = XMMatrixRotationY(MathHelper::Pi);
		XMMATRIX modelOffset = XMMatrixTranslation(0.0f, 0.0f, -5.0f);
		XMStoreFloat4x4(&ritem->World, modelScale * modelRot * modelOffset);

		ritem->TexTransform = MathHelper::Identity4x4();
		ritem->ObjectCBIndex = objCBIndex++;
		ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
		ritem->Geo = mGeometries[mSkinnedModelFilename].get();
		ritem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
		ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
		ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

		ritem->SkinnedCBIndex = 0;
		ritem->SkinnedModelInst = mSkinnedModelInst.get();

		mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
		mAllRitems.push_back(std::move(ritem));
	}
}

void SkinnedMeshApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(), 2,
			(UINT)mAllRitems.size(), 1, (UINT)mMaterials.size()));
	}
}

void SkinnedMeshApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;
	ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	basePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	basePsoDesc.pRootSignature = mRootSignature.Get();
	basePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	basePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	basePsoDesc.NumRenderTargets = 1;
	basePsoDesc.SampleMask = UINT_MAX;
	basePsoDesc.RTVFormats[0] = mBackBufferFormat;
	basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	basePsoDesc.SampleDesc.Count = 1;
	basePsoDesc.SampleDesc.Quality = 0;
	basePsoDesc.DSVFormat = mDepthStencilFormat;

	///
	/// ��͸������PSO, ��Ϊ��Ҫ�������/��������, ������Ͳ���Ҫ�������д��, ��������Ҫ��Ȳ���.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = basePsoDesc;
	opaquePsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	opaquePsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc,
		IID_PPV_ARGS(mPSOs["opaque"].GetAddressOf())));

	///
	/// ��ɫ���Ƶ�PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedOpaquePsoDesc = opaquePsoDesc;
	skinnedOpaquePsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedOpaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedVS"]->GetBufferPointer()),
		mShaders["skinnedVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedOpaquePsoDesc,
		IID_PPV_ARGS(mPSOs["skinnedOpaque"].GetAddressOf())));

	///
	/// ��Ӱ��ͼ���ɵ�PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = basePsoDesc;
	shadowPsoDesc.RasterizerState.DepthBias = 100000;
	shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	shadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()),
		mShaders["shadowVS"]->GetBufferSize()
	};
	shadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()),
		mShaders["shadowPS"]->GetBufferSize()
	};
	shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowPsoDesc.NumRenderTargets = 0;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc,
		IID_PPV_ARGS(mPSOs["shadow"].GetAddressOf())));

	///
	/// ��ɫ��Ӱ��PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedShadowPsoDesc = shadowPsoDesc;
	skinnedShadowPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedShadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedShadowVS"]->GetBufferPointer()),
		mShaders["skinnedShadowVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedShadowPsoDesc,
		IID_PPV_ARGS(mPSOs["skinnedShadow_opaque"].GetAddressOf())));

	///
	/// ��պ�PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = basePsoDesc;
	skyPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc,
		IID_PPV_ARGS(mPSOs["sky"].GetAddressOf())));

	///
	/// �������/���������PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
	drawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsVS"]->GetBufferPointer()),
		mShaders["drawNormalsVS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["drawNormalsPS"]->GetBufferPointer()),
		mShaders["drawNormalsPS"]->GetBufferSize()
	};
	drawNormalsPsoDesc.RTVFormats[0] = SSAO::NormalMapFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&drawNormalsPsoDesc,
		IID_PPV_ARGS(mPSOs["drawNormals"].GetAddressOf())));

	///
	/// ���ƽ�ɫ���/���ߵ�PSO.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC skinnedDrawNormalsPsoDesc = drawNormalsPsoDesc;
	skinnedDrawNormalsPsoDesc.InputLayout = { mSkinnedInputLayout.data(), (UINT)mSkinnedInputLayout.size() };
	skinnedDrawNormalsPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["skinnedDrawNormalsVS"]->GetBufferPointer()),
		mShaders["skinnedDrawNormalsVS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skinnedDrawNormalsPsoDesc,
		IID_PPV_ARGS(mPSOs["skinnedDrawNormals"].GetAddressOf())));

	///
	/// SSAO���Ƶ�PSO. SSAO����Ҫ���д��, Ҳ����Ҫ��Ȳ���, ��������Ϣ�Ѿ��ڴ�ǰ��ȡ.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
	ssaoPsoDesc.InputLayout = { nullptr, 0 };
	ssaoPsoDesc.pRootSignature = mSSAORootSignature.Get();
	ssaoPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoVS"]->GetBufferPointer()),
		mShaders["ssaoVS"]->GetBufferSize()
	};
	ssaoPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoPS"]->GetBufferPointer()),
		mShaders["ssaoPS"]->GetBufferSize()
	};
	ssaoPsoDesc.DepthStencilState.DepthEnable = FALSE;
	ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ssaoPsoDesc.RTVFormats[0] = SSAO::AmbientMapFormat;
	ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&ssaoPsoDesc,
		IID_PPV_ARGS(mPSOs["ssao"].GetAddressOf())));

	///
	/// �������õ�PSO, ��������ʾSSAO����.
	///
	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = basePsoDesc;
	debugPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc,
		IID_PPV_ARGS(mPSOs["debug"].GetAddressOf())));

	///
	/// SSAO����ģ������PSO.
	///
	D3D12_COMPUTE_PIPELINE_STATE_DESC ssaoHorzBlurPsoDesc = {};
	ssaoHorzBlurPsoDesc.pRootSignature = mSSAORootSignature.Get();
	ssaoHorzBlurPsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoHorzBlur"]->GetBufferPointer()),
		mShaders["ssaoHorzBlur"]->GetBufferSize()
	};
	ssaoHorzBlurPsoDesc.NodeMask = 0;
	ssaoHorzBlurPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&ssaoHorzBlurPsoDesc,
		IID_PPV_ARGS(mPSOs["ssaoHorzBlur"].GetAddressOf())));

	///
	/// SSAO����ģ������PSO.
	///
	D3D12_COMPUTE_PIPELINE_STATE_DESC ssaoVertBlurPsoDesc = ssaoHorzBlurPsoDesc;
	ssaoVertBlurPsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["ssaoVertBlur"]->GetBufferPointer()),
		mShaders["ssaoVertBlur"]->GetBufferSize()
	};
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&ssaoVertBlurPsoDesc,
		IID_PPV_ARGS(mPSOs["ssaoVertBlur"].GetAddressOf())));
}

void SkinnedMeshApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	const UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	const UINT skinnedCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(SkinnedConstants));

	for (size_t i = 0; i < ritems.size(); i++)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		auto objCBAddr = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress() +
			ri->ObjectCBIndex * objCBByteSize;
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddr);

		if (ri->SkinnedModelInst != nullptr)
		{
			auto skinnedCBAddress = mCurrFrameResource->SkinnedCB->Resource()->GetGPUVirtualAddress() +
				ri->SkinnedCBIndex * skinnedCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(1, skinnedCBAddress);
		}

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation,
			ri->BaseVertexLocation, 0);
	}
}

void SkinnedMeshApp::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());

	///
	/// ���Ƴ�������Ӱ��ͼ.
	///
	auto passCBAddr = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress() +
		1 * d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddr);

	// Ĭ���Ȼ�����Ӱ��ͼ, ����������PSOʱ�Ѿ����ó�����Ӱ��PSO.
	// mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedShadow_opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SkinnedMeshApp::DrawNormalAndDepthMap()
{
	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAO->NormalMap(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearColor[] = { 0.0f, 0.0f, 1.0f, 0.0f };
	mCommandList->ClearRenderTargetView(mSSAO->NormalMapCPURtv(), clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, nullptr);

	mCommandList->OMSetRenderTargets(1, &mSSAO->NormalMapCPURtv(), true, &DepthStencilView());

	///
	/// ���Ʒ�������.
	///
	auto passCBAddr = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCBAddr);

	mCommandList->SetPipelineState(mPSOs["drawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["skinnedDrawNormals"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::SkinnedOpaque]);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSSAO->NormalMap(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetCpuSrv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		index, mCbvSrvUavDescriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetGpuSrv(int index) const
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(),
		index, mCbvSrvUavDescriptorSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetCpuDsv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
		index, mDsvDescriptorSize);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SkinnedMeshApp::GetCpuRtv(int index) const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		index, mRtvDescriptorSize);
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 8> SkinnedMeshApp::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1,
		D3D12_FILTER_MIN_MAG_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	const CD3DX12_STATIC_SAMPLER_DESC anistropicWrap(
		4,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		0.0f,
		8);

	const CD3DX12_STATIC_SAMPLER_DESC anistropicClamp(
		5,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		0.0f,
		8);

	// ȷ����Ӱ�ıȽϲ�����.
	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

	// ������Ȳ���.
	const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
		7,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		0,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

	return { pointWrap, pointClamp, linearWrap, linearClamp,
		anistropicWrap, anistropicClamp,
		shadow, depthMapSam };
}
