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

    // Slot의 머티리얼을 찾는다. OutMaterialId에는 슬롯 이름의 FName이 들어간다.
    // 이전에 연결한 이름과 Materials[Slot]이 같으면 FName 생성·맵 조회 없이 보관 중인 참조를 쓰고,
    // 이름이 바뀌었거나 머티리얼이 사라졌으면 다시 찾는다. 등록되지 않은 이름이면 nullptr.
    FMaterial* ResolveMaterialSlot(int32 Slot, FName& OutMaterialId) const;

    // 에셋 정보 조회
    const FString& GetAssetPathFileName() const;

    // 메시 에셋 설정
    void SetStaticMeshAsset(TSharedPtr<FStaticMesh> InStaticMesh);
    void SetStaticMeshAsset(FStaticMesh* InStaticMesh);

    UStaticMesh* ClonePreviewMesh() const;

private:
    // Materials[i]를 마지막으로 연결했을 때의 이름과 그 결과.
    // Materials는 여러 곳(에디터 콤보박스 등)에서 직접 수정되므로, 조회할 때 이름을 비교해 맞춘다.
    struct FMaterialSlotRef
    {
        FString SourceName;
        FName MaterialId;
        TWeakPtr<FMaterial> Material;
    };
    mutable TArray<FMaterialSlotRef> MaterialSlotRefs;

    void InitializeFromAsset(const FString& InMaterialId);
    static FName DetermineMaterialId(const FName& FallbackMaterialId, const FName& Diffuse, const FName& Normal, const FName& Specular);
};
