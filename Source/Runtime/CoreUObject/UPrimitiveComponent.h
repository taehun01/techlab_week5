#pragma once

#include "Runtime/Engine/FCamera.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Rendering/FRenderQueue.h"
#include "Runtime/Engine/ShowFlags.h"
#include "USceneComponent.h"

class UPrimitiveComponent : public USceneComponent {
  GENERATED_BODY()
  DECLARE_UCLASS(UPrimitiveComponent, USceneComponent)

public:
    void Initialize() override;
    void Register(UScene& InScene) override;
    void Unregister() override;

    // 트랜스폼 관리
    FTransform& GetRelativeTransform() { return RelativeTransform; }
    const FTransform& GetRelativeTransform() const { return RelativeTransform; }
    virtual void SetRelativeTransform(const FTransform& InRelativeTransform);
    FTransform GetGlobalTransform() const;

    virtual FMatrix GetRenderMatrix(const FCamera& Camera) const { return GetGlobalTransform().ToMatrix(); }
    FMatrix GetModelMatrix();

    // 직렬화
    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;


    // 충돌 판정용 바운드 계산
    virtual FAxisAlignedBoundingBox CalcLocalBounds();

    // 색상 설정 및 조회
    const FVector& GetColor() const { return Color; }
    void SetColor(const FVector& InColor) {
        Color = InColor;
        ColorAmount = 1.0f;
    }
    float GetColorAmount() const { return ColorAmount; }
    void SetColorAmount(float InAmount) { ColorAmount = InAmount; }

    virtual EEngineShowFlags GetShowFlag() const { return EEngineShowFlags::SF_Primitives; }

protected:
    UPrimitiveComponent() = default;

    FTransform RelativeTransform;
    FVector Color{1.0f, 1.0f, 1.0f};
    float ColorAmount = 0.0f;
};
