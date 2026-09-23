#include "UStaticMesh.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "UClass.h"

IMPLEMENT_UCLASS(UStaticMesh, UObject)

UStaticMesh::UStaticMesh(const FName& InMeshId, const FName& InMaterialId)
    : MeshId(InMeshId)
{
    StaticMeshAsset = FRenderResourceLibrary::Get().GetMesh(InMeshId);
    if (!StaticMeshAsset)
    {
        return;
    }

    LocalBounds = StaticMeshAsset->GetLocalBounds();
    if(!InMaterialId.IsNone())
        InitializeFromAsset(InMaterialId.ToString());
}

void UStaticMesh::InitializeFromAsset(const FString& InMaterialId)
{
    Materials.clear();
    Materials.push_back(InMaterialId);
}

FName UStaticMesh::DetermineMaterialId(const FName& FallbackMaterialId, const FName& Diffuse, const FName& Normal, const FName& Specular)
{
    if (!FallbackMaterialId.IsNone() && FallbackMaterialId != FName("None") && FallbackMaterialId != FName("Simple"))
    {
        return FallbackMaterialId;
    }

    const bool bHasAnyTexture = (!Diffuse.IsNone() && Diffuse != FName("None")) ||
                                (!Normal.IsNone() && Normal != FName("None")) ||
                                (!Specular.IsNone() && Specular != FName("None"));

    return bHasAnyTexture ? FName("Textured") : FName("Simple");
}

int32 UStaticMesh::GetMaterialSlotCount() const
{
    return static_cast<int32>(Materials.size());
}


const FString& UStaticMesh::GetAssetPathFileName() const
{
    static const FString EmptyString;
    return StaticMeshAsset ? StaticMeshAsset->PathFileName : EmptyString;
}

void UStaticMesh::SetStaticMeshAsset(TSharedPtr<FStaticMesh> InStaticMesh)
{
    StaticMeshAsset = InStaticMesh;
    if (StaticMeshAsset)
    {
        LocalBounds = StaticMeshAsset->GetLocalBounds();
    }
    else
    {
        LocalBounds = FAxisAlignedBoundingBox{};
        Materials.clear();
    }
}

void UStaticMesh::SetStaticMeshAsset(FStaticMesh* InStaticMesh)
{
    SetStaticMeshAsset(TSharedPtr<FStaticMesh>(InStaticMesh));
}


UStaticMesh* UStaticMesh::ClonePreviewMesh() const
{
    UStaticMesh* ClonedMesh = NewObject<UStaticMesh>();
    ClonedMesh->MeshId = this->MeshId;
    ClonedMesh->StaticMeshAsset = this->StaticMeshAsset; // 버퍼는 그대로 공유
    ClonedMesh->Materials = this->Materials;             // 슬롯 배열 복제
    ClonedMesh->LocalBounds = this->LocalBounds;
    return ClonedMesh;
}