#include "FPreviewRenderTarget.h"

bool FPreviewRenderTarget::Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
	if (!Device || InWidth == 0 || InHeight == 0)
	{
		return false;
	}

	Release();

	Width = InWidth;
	Height = InHeight;

	// 컬러 텍스처 생성
	D3D11_TEXTURE2D_DESC ColorDesc = {};
	ColorDesc.Width = Width;
	ColorDesc.Height = Height;
	ColorDesc.MipLevels = 1;
	ColorDesc.ArraySize = 1;
	ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ColorDesc.SampleDesc.Count = 1;
	ColorDesc.Usage = D3D11_USAGE_DEFAULT;
	ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	HRESULT Hr = Device->CreateTexture2D(&ColorDesc, nullptr, &ColorTexture);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	// 렌더타겟 뷰 생성
	Hr = Device->CreateRenderTargetView(ColorTexture.Get(), nullptr, &RenderTargetView);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	// 셰이더 리소스 뷰 생성
	Hr = Device->CreateShaderResourceView(ColorTexture.Get(), nullptr, &ShaderResourceView);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	// 깊이 스텐실 텍스처 생성
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Hr = Device->CreateTexture2D(&DepthDesc, nullptr, &DepthStencilTexture);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	// 깊이 스텐실 뷰 생성
	Hr = Device->CreateDepthStencilView(DepthStencilTexture.Get(), nullptr, &DepthStencilView);
	if (FAILED(Hr))
	{
		Release();
		return false;
	}

	return true;
}

bool FPreviewRenderTarget::Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight)
{
	if (Width == InWidth && Height == InHeight && IsValid())
	{
		return true;
	}

	// 현재 프레임 ImGui 드로우 리스트가 참조 중인 경우를 대비해 이전 뷰 보존
	OldShaderResourceView = ShaderResourceView;
	return Initialize(Device, InWidth, InHeight);
}

void FPreviewRenderTarget::Release()
{
	RenderTargetView.Reset();
	ShaderResourceView.Reset();
	ColorTexture.Reset();

	DepthStencilView.Reset();
	DepthStencilTexture.Reset();

	Width = 0;
	Height = 0;
}
