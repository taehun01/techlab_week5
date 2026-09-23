#include "UInstancePrimitiveComponent.h"
#include "Runtime/Engine/UScene.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "UClass.h"


IMPLEMENT_UCLASS(UInstancePrimitiveComponent, UStaticMeshComponent)

void UInstancePrimitiveComponent::Initialize() {
  Super::Initialize();

  // SetMeshID(FName("Cube"));

  if (GetMaterialID().IsNone()) {
    SetMaterialID(FName("Instance_Simple"));
  } else if (GetMaterialID() == FName("Textured")) {
    SetMaterialID(FName("Instance_Textured"));
  }

  // 텍스처 ID가 지정되어 있고 머티리얼이 있는 경우 텍스처 설정
  if (!GetTextureID().IsNone() && GetTextureID() != FName("None")) {
    auto Mat = FRenderResourceLibrary::Get().GetMaterial(GetMaterialID());
    if (Mat) {
      auto Tex = FRenderResourceLibrary::Get().GetTexture(GetTextureID());
      if (Tex) {
        Mat->SetTexture(Tex);
      }
    }
  }
}

void UInstancePrimitiveComponent::AddInstance(const FVector &WorldPosition,
                                              const FVector4 &Color) {
  InstanceTransforms.push_back({WorldPosition, Color});
}

void UInstancePrimitiveComponent::ClearInstances() {
  InstanceTransforms.clear();
}

void UInstancePrimitiveComponent::BuildRenderData() {
  TArray<FInstanceData> Built;
  const FTransform BaseTransform = GetGlobalTransform();

  if (InstanceTransforms.empty()) {
    // 등록된 인스턴스 없으면 자기 자신 트랜스폼 적용
    Built.push_back(FInstanceData{
        .World = BaseTransform.ToMatrix(),
        .Color = FVector4(GetColor(), 1.0f),
        .UVScale = {1.0f, 1.0f},
        .UVOffset = {0.0f, 0.0f},
    });
  } else {
    Built.reserve(InstanceTransforms.size());
    for (const auto &Entry : InstanceTransforms) { 
      FTransform InstTransform = BaseTransform;
      InstTransform.Location = Entry.Position;
      Built.push_back(FInstanceData{
          .World = InstTransform.ToMatrix(),
          .Color = Entry.Color,
          .UVScale = {1.0f, 1.0f},
          .UVOffset = {0.0f, 0.0f},
      });
    }
  }

  RenderDatas.at(0).Instances = std::move(Built);
}

TArray<FRenderData> UInstancePrimitiveComponent::GetRenderDatas(const FCamera& Camera)
{
    BuildRenderData();
    return RenderDatas;
}

