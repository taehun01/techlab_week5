#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include "Runtime/Core/IntTypes.h"

// 프리뷰 화면 렌더링을 위한 렌더타겟 구조체
struct FPreviewRenderTarget
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> ColorTexture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ShaderResourceView;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> OldShaderResourceView;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> DepthStencilTexture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DepthStencilView;

	uint32 Width = 0;
	uint32 Height = 0;

	// 렌더타겟 및 깊이버퍼 생성
	bool Initialize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);

	// 해상도 변경 시 리소스 재생성
	bool Resize(ID3D11Device* Device, uint32 InWidth, uint32 InHeight);

	// 모든 리소스 해제
	void Release();

	// 유효성 확인
	[[nodiscard]] bool IsValid() const { return RenderTargetView != nullptr; }
};
