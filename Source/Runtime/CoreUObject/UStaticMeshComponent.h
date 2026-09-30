#pragma once

#include "UMeshComponent.h"
#include "UStaticMesh.h"
#include <Runtime\Core\TArray.h>

class UStaticMeshComponent : public UMeshComponent {
    GENERATED_BODY()
    DECLARE_UCLASS(UStaticMeshComponent, UMeshComponent)

public:
    void Initialize() override;
    void Update(float DeltaTime) override;

    // StaticMesh 에셋 설정 및 조회
    bool SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }

    // UMeshComponent 식별자 오버라이드
    const FName& GetMeshID() const override;
    FName GetMaterialID() const override;
    FAxisAlignedBoundingBox CalcLocalBounds() override;
    void UpdateLocalBounds() override;

    TArray<FRenderData> GetRenderDatas(const FCamera& Camera) override;
    void AppendRenderDatas(const FCamera& Camera, TArray<FRenderData>& OutDatas, uint32 LodLevel = 0u) override;
    const FRenderData& GetPureRenderData() const override;

    // 머티리얼 오버라이드
    void SetMaterial(int32 Slot, const FName& InMaterialId);
    FName GetMaterial(int32 Slot = 0) const;

    bool bIsMovingUV = false;
    float UVSpeed = 0.1f;

    virtual void Serialize(FArchive& Archive) const override;
    virtual void Deserialize(const FArchive& Archive) override;

    virtual FStaticMesh* GetFStaticMesh() const override { return StaticMesh ? StaticMesh->StaticMeshAsset.get() : nullptr; }

protected:
    UStaticMeshComponent() = default;

    // GetMaterial(Slot)과 같은 규칙으로 머티리얼을 정해 OutData에 채운다.
    // 메시 슬롯의 머티리얼이면 포인터까지 넘겨 렌더러의 이름 조회를 생략한다.
    void FillMaterial(int32 Slot, FRenderData& OutData) const;

    UStaticMesh* StaticMesh = nullptr;
    TArray<FName> OverrideMaterials;

    float offset = 0.0f;
    
};
