#include "SSAO.h"

SSAO::SSAO(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, UINT width, UINT height)
{
	md3dDevice = device;

	OnResize(width, height);

	BuildOffsetVectors();
	BuildRandomVectorTexture(cmdList);
}

UINT SSAO::SSAOMapWidth() const
{
	return mRenderTargetWidth / 2;
}

UINT SSAO::SSAOMapHeight() const
{
	return mRenderTargetHeight / 2;
}

void SSAO::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14])
{
	std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
}

std::vector<float> SSAO::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2 * sigma * sigma;
	int blurRadius = (int)ceilf(2.0f * sigma);

	assert(blurRadius <= MaxBlurRadius);

	std::vector<float> weights(2 * blurRadius + 1);
	float weightsSum = 0;

	for (int i = -blurRadius; i <=blurRadius; ++i)
	{
		float x = (float)i;
		weights[i + blurRadius] = expf(-x * x / twoSigma2);
		weightsSum += weights[i + blurRadius];
	}

	for (int i = 0; i < (int)weights.size(); ++i)
	{
		weights[i] /= weightsSum;
	}

	return weights;
}

ID3D12Resource* SSAO::AmbientMap() const
{
	return mAmbientMap0.Get();
}

ID3D12Resource* SSAO::NormalMap() const
{
	return mNormalMap.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE SSAO::NormalMapCPURtv() const
{
	return mhNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::NormalMapGPUSrv() const
{
	return mhNormalMapGpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE SSAO::AmbientMapGPUSrv() const
{
	return mhAmbientMap0GpuSrv;
}

void SSAO::BuildDescriptors(ID3D12Resource* depthStencilBuffer,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, 
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, 
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, 
	UINT cbvSrvUavDescSize, 
	UINT rtvDescSize)
{
	mhAmbientMap0CpuSrv = hCpuSrv;
	mhAmbientMap1CpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescSize);
	mhNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescSize);
	mhDepthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescSize);
	mhRandomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescSize);

	mhAmbientMap0GpuSrv = hGpuSrv;
	mhAmbientMap1GpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescSize);
	mhNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescSize);
	mhDepthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescSize);
	mhRandomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescSize);

	mhNormalMapCpuRtv = hCpuRtv;
	mhAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescSize);
	mhAmbientMap1CpuRtv = hCpuRtv.Offset(1, rtvDescSize);

	RebuildDescriptors(depthStencilBuffer);
}

void SSAO::RebuildDescriptors(ID3D12Resource* depthStencilBuffer)
{
	/// 在这里深度纹理、法线纹理、AO纹理是相互关联绘制的, 因此每当发生改变, 
	/// 相关数据就需要重新构建一次.

	///
	/// SRV描述符重构.
	///
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	// 法线纹理.
	srvDesc.Format = NormalMapFormat;
	md3dDevice->CreateShaderResourceView(NormalMap(), &srvDesc, mhNormalMapCpuSrv);

	// 深度纹理.
	srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	md3dDevice->CreateShaderResourceView(depthStencilBuffer, &srvDesc, mhDepthMapCpuSrv);

	// 随机向量纹理.
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	md3dDevice->CreateShaderResourceView(mRandomVectorMap.Get(), &srvDesc, mhRandomVectorMapCpuSrv);

	// AO.
	srvDesc.Format = AmbientMapFormat;
	md3dDevice->CreateShaderResourceView(mAmbientMap0.Get(), &srvDesc, mhAmbientMap0CpuSrv);
	md3dDevice->CreateShaderResourceView(mAmbientMap1.Get(), &srvDesc, mhAmbientMap1CpuSrv);


	///
	/// RTV描述符重构.
	///
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;

	rtvDesc.Format = NormalMapFormat;
	md3dDevice->CreateRenderTargetView(mNormalMap.Get(), &rtvDesc, mhNormalMapCpuRtv);

	rtvDesc.Format = AmbientMapFormat;
	md3dDevice->CreateRenderTargetView(mAmbientMap0.Get(), &rtvDesc, mhAmbientMap0CpuRtv);
	md3dDevice->CreateRenderTargetView(mAmbientMap1.Get(), &rtvDesc, mhAmbientMap1CpuRtv);
}

void SSAO::SetPSOs(ID3D12PipelineState* ssaoPso, ID3D12PipelineState* ssaoBlurPso)
{
	mSSAOPso = ssaoPso;
	mSSAOBlurPso = ssaoBlurPso;
}

void SSAO::OnResize(UINT newWidth, UINT newHeight)
{
	if (mRenderTargetWidth != newWidth || mRenderTargetHeight != newHeight)
	{
		mRenderTargetWidth = newWidth;
		mRenderTargetHeight = newHeight;

		mViewport = { 0.0f, 0.0f, mRenderTargetWidth / 2.0f, mRenderTargetHeight / 2.0f, 0.0f, 1.0f };
		mScissorRect = { 0, 0, (int)mRenderTargetWidth / 2, (int)mRenderTargetHeight / 2 };

		BuildResources();
	}
}

void SSAO::ComputeSSAO(ID3D12GraphicsCommandList* cmdList, FrameResource* currentFrame, int blurCount)
{
	cmdList->RSSetViewports(1, &mViewport);
	cmdList->RSSetScissorRects(1, &mScissorRect);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);

	cmdList->SetGraphicsRootConstantBufferView(0, currentFrame->SSAOCB->Resource()->GetGPUVirtualAddress());
	cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);

	cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv);

	cmdList->SetPipelineState(mSSAOPso);

	///
	/// 基于屏幕后处理实现绘制, 将SSAO绘制到近平面.
	///
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));

	BlurAmbientMap(cmdList, currentFrame, blurCount);
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, FrameResource* currentFrame, int blurCount)
{
	cmdList->SetPipelineState(mSSAOBlurPso);
	cmdList->SetGraphicsRootConstantBufferView(0, currentFrame->SSAOCB->Resource()->GetGPUVirtualAddress());

	for (int i = 0; i < blurCount; ++i)
	{
		// 先执行横向模糊, 再执行纵向模糊, 这个顺序固定, 不能交换.
		BlurAmbientMap(cmdList, true);
		BlurAmbientMap(cmdList, false);
	}
}

void SSAO::BlurAmbientMap(ID3D12GraphicsCommandList* cmdList, bool horzBlur)
{
	ID3D12Resource* output = nullptr;
	CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE outputRtv;

	// 判断模糊的方向, 将两个方向模糊相同代码提取, 提高阅读质量.
	if (horzBlur == true)
	{
		output = mAmbientMap1.Get();
		inputSrv = mhAmbientMap0GpuSrv;
		outputRtv = mhAmbientMap1CpuRtv;

		cmdList->SetGraphicsRoot32BitConstant(1, 1, 0);
	}
	else
	{
		output = mAmbientMap0.Get();
		inputSrv = mhAmbientMap1GpuSrv;
		outputRtv = mhAmbientMap0CpuRtv;

		cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);
	}

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	cmdList->ClearRenderTargetView(outputRtv, clearValue, 0, nullptr);
	cmdList->OMSetRenderTargets(1, &outputRtv, true, nullptr);

	cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);
	cmdList->SetGraphicsRootDescriptorTable(3, inputSrv);

	///
	/// 在这里模糊也是基于屏幕后处理实现的.
	///
	cmdList->IASetVertexBuffers(0, 0, nullptr);
	cmdList->IASetIndexBuffer(nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->DrawInstanced(6, 1, 0, 0);

	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(output,
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void SSAO::BuildResources()
{
	// 先将之前的资源清空.
	mNormalMap = nullptr;
	mAmbientMap0 = nullptr;
	mAmbientMap1 = nullptr;

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.MipLevels = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	
	///
	/// 法线纹理资源创建.
	///
	texDesc.Format = NormalMapFormat;
	texDesc.Width = mRenderTargetWidth;
	texDesc.Height = mRenderTargetHeight;

	D3D12_CLEAR_VALUE normalMapClearValue;
	normalMapClearValue.Format = NormalMapFormat;
	normalMapClearValue.Color[0] = 0.0f;
	normalMapClearValue.Color[1] = 0.0f;
	normalMapClearValue.Color[2] = 1.0f;
	normalMapClearValue.Color[3] = 0.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&normalMapClearValue,
		IID_PPV_ARGS(mNormalMap.ReleaseAndGetAddressOf())));

	///
	/// SSAO纹理资源.
	///
	texDesc.Format = AmbientMapFormat;
	texDesc.Width = mRenderTargetWidth / 2;
	texDesc.Height = mRenderTargetHeight / 2;

	D3D12_CLEAR_VALUE ambientClear;
	ambientClear.Format = AmbientMapFormat;
	ambientClear.Color[0] = 1.0f;
	ambientClear.Color[1] = 1.0f;
	ambientClear.Color[2] = 1.0f;
	ambientClear.Color[3] = 1.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&ambientClear,
		IID_PPV_ARGS(mAmbientMap0.ReleaseAndGetAddressOf())));

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&ambientClear,
		IID_PPV_ARGS(mAmbientMap1.ReleaseAndGetAddressOf())));
}

void SSAO::BuildRandomVectorTexture(ID3D12GraphicsCommandList* cmdList)
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Alignment = 0;
	texDesc.DepthOrArraySize = 1;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.MipLevels = 1;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Width = 256;
	texDesc.Height = 256;

	// 创建默认堆, 资源最终会通过上传堆上传到GPU.
	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mRandomVectorMap.GetAddressOf())));

	///
	/// 将相关资源提交到上传堆, 上传堆再传递到默认堆.
	///
	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap.Get(), 0, num2DSubresources);

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mRandomVectorMapUploadBuffer.GetAddressOf())));

	// 在这里使用的普通指针, 需要注意清理.
	XMCOLOR* initData = new XMCOLOR[256 * 256];
	for (int i = 0; i < 256; ++i)
	{
		for (int j = 0; j < 256; ++j)
		{
			initData[i * 256 + j] = XMCOLOR(MathHelper::RandF(), MathHelper::RandF(),
				MathHelper::RandF(), 0.0f);
		}
	}

	D3D12_SUBRESOURCE_DATA subresourcesData = {};
	subresourcesData.pData = initData;
	subresourcesData.RowPitch = 256 * sizeof(XMCOLOR);
	subresourcesData.SlicePitch = subresourcesData.RowPitch * 256;

	// 上传资源.
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(cmdList, mRandomVectorMap.Get(), mRandomVectorMapUploadBuffer.Get(),
		0, 0, num2DSubresources, &subresourcesData);
	cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	// 避免内存泄漏.
	delete[] initData;
}

void SSAO::BuildOffsetVectors()
{
	// 正方体的8个顶点.
	mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
	mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

	mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

	mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
	mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

	// 正方体的6个面的中心点.
	mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
	mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

	mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
	mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

	mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
	mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

	for (int i = 0; i < 14; ++i)
	{
		float s = MathHelper::RandF(0.25f, 1.0f);
		XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));
		XMStoreFloat4(&mOffsets[i], v);
	}
}
