#pragma once

#include "FRenderPipeline.h"
#include "FTexture.h"
#include "Runtime/Core/FString.h"
#include "Runtime/Core/FName.h"
#include "Runtime/Core/PointerTypes.h"
#include "Runtime/Core/TMap.h"
#include "Vertices.h"
#include <d3d11.h>


class FRenderer;
class FRenderResourceLibrary;



// 텍스처 맵 슬롯 구분
enum class EMaterialTextureSlot : uint32
{
  Diffuse = 0,
  Normal = 1,
  Specular = 2,
  Count = 3
};

class FMaterial final {
  friend class FRenderer;

public:
  FMaterial() = default;
  
  void SetPipeLine(const TSharedPtr<FRenderPipeline>& InPipeline);

  TSharedPtr<FMaterial> Clone() const;

  [[nodiscard]] TSharedPtr<FRenderPipeline> GetPipeline() const { return Pipeline; }

  // 슬롯별 텍스처 설정
  void SetTexture(EMaterialTextureSlot Slot, const TSharedPtr<FTexture>& InTexture);
  [[nodiscard]] TSharedPtr<FTexture> GetTexture(EMaterialTextureSlot Slot) const;
  bool SetTextureByName(EMaterialTextureSlot Slot, const FName& InTextureName);

  // 개별 맵 설정
  void SetDiffuseMap(const TSharedPtr<FTexture>& InTexture) { SetTexture(EMaterialTextureSlot::Diffuse, InTexture); }
  void SetNormalMap(const TSharedPtr<FTexture>& InTexture) { SetTexture(EMaterialTextureSlot::Normal, InTexture); }
  void SetSpecularMap(const TSharedPtr<FTexture>& InTexture) { SetTexture(EMaterialTextureSlot::Specular, InTexture); }

  [[nodiscard]] TSharedPtr<FTexture> GetDiffuseMap() const { return GetTexture(EMaterialTextureSlot::Diffuse); }
  [[nodiscard]] TSharedPtr<FTexture> GetNormalMap() const { return GetTexture(EMaterialTextureSlot::Normal); }
  [[nodiscard]] TSharedPtr<FTexture> GetSpecularMap() const { return GetTexture(EMaterialTextureSlot::Specular); }

  bool SetDiffuseMapByName(const FName& InTextureName) { return SetTextureByName(EMaterialTextureSlot::Diffuse, InTextureName); }
  bool SetNormalMapByName(const FName& InTextureName) { return SetTextureByName(EMaterialTextureSlot::Normal, InTextureName); }
  bool SetSpecularMapByName(const FName& InTextureName) { return SetTextureByName(EMaterialTextureSlot::Specular, InTextureName); }

  // 기존 단일 텍스처 호환 인터페이스
  void SetTexture(const TSharedPtr<FTexture>& InTexture) { SetDiffuseMap(InTexture); }
  [[nodiscard]] TSharedPtr<FTexture> GetTexture() const { return GetDiffuseMap(); }
  bool SetTextureByName(const FName& InTextureName) { return SetDiffuseMapByName(InTextureName); }

  // 파이프라인 블렌드 모드 조회
  [[nodiscard]] EBlendMode GetBlendMode() const {
    return Pipeline ? Pipeline->GetPipelineDesc().BlendMode : EBlendMode::Opaque;
  }

  FString MaterialId;
private:
  void BindResources(ID3D11DeviceContext &Context) const;

  TSharedPtr<FRenderPipeline> Pipeline;
  TSharedPtr<FRenderPipeline> WireframePipeline;

  TSharedPtr<FTexture> Textures[static_cast<size_t>(EMaterialTextureSlot::Count)];
};



struct FMaterialDesc {
  FWString VertexShaderFileName;
  FWString PixelShaderFileName;
  bool bEnableDepthTest = true;
};
