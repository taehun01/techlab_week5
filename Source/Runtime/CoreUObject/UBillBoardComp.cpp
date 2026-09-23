#include "UBillBoardComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Engine/UScene.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Core/Log.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Engine/FSceneView.h"
#include "UClass.h"
#include <algorithm>
#include <cctype>

IMPLEMENT_UCLASS(UBillBoardComp, UMeshComponent)
UCLASS_META(UBillBoardComp, DisplayName, "BillBoard")
UCLASS_META(UBillBoardComp, MeshName, "BillBoard")

void UBillBoardComp::Initialize() {
  Super::Initialize();
  SetMeshID(FName("Rect"));
  SetMaterialID(FName("Billboard"));
}

void UBillBoardComp::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);

    Archive.SetVector2("UVScale", UVScale);
    Archive.SetVector2("UVOffset", UVOffset);
}

void UBillBoardComp::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);

    UVScale = Archive.GetVector2("UVScale");
    UVOffset = Archive.GetVector2("UVOffset");
}


void UBillBoardComp::SetTexture(
    FString texture) // 원본 머터리얼을 건드리지 않고 instance로 생성해서 사용
{
  auto &lib = FRenderResourceLibrary::Get();

  // 소문자 변환
  FString LowerName = texture;
  std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(),
                 ::tolower);

  FName TextureId(LowerName);
  auto NewTex = lib.GetTexture(TextureId);
  if (!NewTex) {
    UE_LOG("There is no such texture");
    return;
  }

  // TextureId를 RenderData에 기록
  RenderDatas.at(0).TextureId = TextureId;
}

FMatrix UBillBoardComp::GetRenderMatrix(const FCamera& Camera) const
{
    FTransform Transform = GetGlobalTransform();

    FMatrix CameraRotation = Camera.GetRotationMatrix();
    FVector ViewForward = CameraRotation.TransformPointRow(FVector{ 1.0f, 0.0f, 0.0f }, 0.0f); // X+
    FVector ViewRight = CameraRotation.TransformPointRow(FVector{ 0.0f, 1.0f, 0.0f }, 0.0f); // Y+
    FVector ViewUp = CameraRotation.TransformPointRow(FVector{ 0.0f, 0.0f, 1.0f }, 0.0f); // Z+

    FVector Up = ViewUp * Transform.Scale3D.Z;
    FVector Right = ViewRight * Transform.Scale3D.Y;

    return FMatrix
    {
        FVector4{ ViewForward, 0.0f },
        FVector4{ Right, 0.0f },
        FVector4{ Up, 0.0f },
        FVector4{ Transform.Location, 1.0f },
    };
}
