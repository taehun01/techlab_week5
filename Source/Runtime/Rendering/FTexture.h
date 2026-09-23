#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Runtime/Core/IntTypes.h"

class FRenderResourceLibrary;

class FTexture final
{
	friend class FRenderer;
	friend class FMaterial;
	friend class FRenderResourceLibrary;

public:
	[[nodiscard]] ID3D11ShaderResourceView* GetSRV() const { return TextureSRV.Get(); }
	[[nodiscard]] uint32 GetWidth() const { return Width; }
	[[nodiscard]] uint32 GetHeight() const { return Height; }

public:
	FTexture() = default;

	uint32 Width = 0u;
	uint32 Height = 0u;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture2D;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
};

struct FTextureDesc
{
	const void* PixelData = nullptr;
	uint32 Width = 0u;
	uint32 Height = 0u;
	uint32 RowPitch = 0u;
};