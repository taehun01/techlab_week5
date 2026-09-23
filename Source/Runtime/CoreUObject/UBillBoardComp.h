#pragma once
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Math/FQuaternion.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "UMeshComponent.h"

class UScene;
class FArchive;

class UBillBoardComp : public UMeshComponent {
  DECLARE_UCLASS(UBillBoardComp, UMeshComponent)
  GENERATED_BODY()

protected:
  explicit UBillBoardComp() = default;

  virtual void Serialize(FArchive& Archive) const;
  virtual void Deserialize(const FArchive& Archive);

  // 텍스처 좌표 속성
  FVector2 UVScale{1.0f, 1.0f};
  FVector2 UVOffset{0.0f, 0.0f};

public:
  void Initialize() override;


  virtual void SetTexture(FString texture); // 원본 머터리얼을 건드리지 않고
                                            // instance로 생성해서 사용

  // Object -> World 변환 행렬 생성
  virtual FMatrix GetRenderMatrix(const FCamera& Camera) const override;
  

  virtual TArray<FRenderData> GetRenderDatas(const FCamera& Camera) {
      RenderDatas.at(0).Constants.UVScale = UVScale;
      RenderDatas.at(0).Constants.UVOffset = UVOffset;
      return RenderDatas;
  }
  virtual const FRenderData& GetPureRenderData() const {
      return RenderDatas.at(0);
  }



  virtual EEngineShowFlags GetShowFlag() const { return EEngineShowFlags::SF_BillboardText; }
private:
  // 시선 회전 보간용 쿼터니언
  FQuaternion CurrentRotation = FQuaternion::Identity();
};

