#pragma once

#include "EBlendMode.h"
#include "Runtime/Core/FString.h"
#include "Vertices.h"
#include <d3d11.h>
#include <wrl/client.h>

#include "Runtime/Core/IntTypes.h"

// 내장 파이프라인 식별자 전방선언

struct FRenderPipelineDesc {
  FWString VertexShaderFileName;
  FWString PixelShaderFileName;
  bool bEnableDepthTest = true;
  bool bEnableDepthWrite = true;               //기본 불투명
  D3D11_CULL_MODE CullMode = D3D11_CULL_BACK;  //기본 뒷면 제거
  EBlendMode BlendMode = EBlendMode::Opaque;
  bool bIsInstancing = false;				   //기본 노 인스턴스

  bool operator==(const FRenderPipelineDesc &) const = default;
};

class FRenderResourceLibrary;

class FRenderPipeline final {
  friend class FRenderer;
  friend class FLineBatcher;
  friend class FRenderResourceLibrary;

public:
  [[nodiscard]] FRenderPipelineDesc GetPipelineDesc() const { return desc; }
  void SetStencilRef(UINT InRef) { StencilRef = InRef; }
  [[nodiscard]] UINT GetStencilRef() const { return StencilRef; }

private:
  FRenderPipelineDesc desc;
  UINT StencilRef = 0;

  void Bind(ID3D11DeviceContext &Context) const;

  Microsoft::WRL::ComPtr<ID3D11VertexShader> VertexShader;
  Microsoft::WRL::ComPtr<ID3D11PixelShader> PixelShader;
  Microsoft::WRL::ComPtr<ID3D11InputLayout> InputLayout;

  Microsoft::WRL::ComPtr<ID3D11RasterizerState> RasterizerState;
  Microsoft::WRL::ComPtr<ID3D11DepthStencilState> DepthStencilState;
  Microsoft::WRL::ComPtr<ID3D11SamplerState> SamplerState;
  Microsoft::WRL::ComPtr<ID3D11BlendState> BlendState;
};
