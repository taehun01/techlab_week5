#pragma once

#include "UMeshComponent.h"
#include "UStaticMesh.h"
#include <Runtime\Core\TArray.h>

class UStaticMeshComponent : public UMeshComponent {
    GENERATED_BODY()
    DECLARE_UCLASS(UStaticMeshComponent, UMeshComponent)

public:
    void Initialize() override;

    // StaticMesh 에셋 설정 및 조회
    bool SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }

    // UMeshComponent 식별자 오버라이드
    const FName& GetMeshID() const override;
    FName GetMaterialID() const override;
    FAxisAlignedBoundingBox CalcLocalBounds() override;


    TArray<FRenderData> GetRenderDatas(const FCamera& Camera) override;
    const FRenderData& GetPureRenderData() const override;

    // 머티리얼 오버라이드
    void SetMaterial(int32 Slot, const FName& InMaterialId);
    FName GetMaterial(int32 Slot = 0) const;

    bool bIsMovingUV = false;
    float UVSpeed = 0.1f;

    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;

protected:
    UStaticMeshComponent() = default;

    UStaticMesh* StaticMesh = nullptr;
    TArray<FName> OverrideMaterials;

    float offset = 0.0f;
    
};
