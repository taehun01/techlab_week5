#pragma once

#include "Runtime/CoreUObject/UObject.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Rendering/FMaterial.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"

class FStaticMesh;

// 머티리얼 슬롯 정보

class UStaticMesh : public UObject
{
    GENERATED_BODY()
    DECLARE_UCLASS(UStaticMesh, UObject)

public:
    UStaticMesh() = default;
    UStaticMesh(const FName& InMeshId, const FName& InMaterialId = FName("None"));

    // 렌더 메시
    FName MeshId{ "None" };
    TSharedPtr<FStaticMesh> StaticMeshAsset = nullptr;

    // 슬롯 목록
    TArray<FString> Materials;


    // 바운딩 박스
    FAxisAlignedBoundingBox LocalBounds{};

    // 유효성 확인
    bool IsValid() const { return StaticMeshAsset != nullptr; }

    // 접근자
    const FAxisAlignedBoundingBox& GetBounds() const { return LocalBounds; }
    TSharedPtr<FStaticMesh> GetStaticMeshAsset() const { return StaticMeshAsset; }
    int32 GetMaterialSlotCount() const;

    // 에셋 정보 조회
    const FString& GetAssetPathFileName() const;

    // 메시 에셋 설정
    void SetStaticMeshAsset(TSharedPtr<FStaticMesh> InStaticMesh);
    void SetStaticMeshAsset(FStaticMesh* InStaticMesh);

    UStaticMesh* ClonePreviewMesh() const;

private:
    void InitializeFromAsset(const FString& InMaterialId);
    static FName DetermineMaterialId(const FName& FallbackMaterialId, const FName& Diffuse, const FName& Normal, const FName& Specular);
};
