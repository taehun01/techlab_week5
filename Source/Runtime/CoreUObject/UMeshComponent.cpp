#include "UMeshComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "UClass.h"
#include "Runtime/Engine/UScene.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/FObjectConstantStore.h"

IMPLEMENT_UCLASS(UMeshComponent, UPrimitiveComponent)

void UMeshComponent::Initialize()
{
    RenderDatas.reserve(1);
    RenderDatas.push_back({
        .MeshId = FName("None"),
        .MaterialId = FName("None"),
        .TextureId = FName("None"),
        .bSelected = false
        });

    Super::Initialize();
}

void UMeshComponent::Register(UScene& InScene)
{
    Super::Register(InScene);
    InScene.AddRenderComponent(this);

    // 렌더 목록에 들어가는 동안 쓸 영구 상수 슬롯
    if (UsesPersistentConstants() && PersistentConstantSlot == FObjectConstantStore::InvalidSlot)
    {
        PersistentConstantSlot = FObjectConstantStore::Get().Allocate();
    }
}

void UMeshComponent::Unregister()
{
    if (Scene)
    {
        Scene->RemoveRenderComponent(this);
    }
    if (PersistentConstantSlot != FObjectConstantStore::InvalidSlot)
    {
        FObjectConstantStore::Get().Free(PersistentConstantSlot);
        PersistentConstantSlot = FObjectConstantStore::InvalidSlot;
    }
    Super::Unregister();
}

FAxisAlignedBoundingBox UMeshComponent::CalcLocalBounds()
{
    if (RenderDatas.empty())
    {
        return Super::CalcLocalBounds();
    }

    if (const TSharedPtr<FStaticMesh> Mesh = FRenderResourceLibrary::Get().GetMesh(GetMeshID()))
    {
        return Mesh->GetLocalBounds();
    }
    return Super::CalcLocalBounds();
}

bool UMeshComponent::SetTextureByName(const FName& InTextureName)
{
    RenderDatas.at(0).TextureId = InTextureName;
    return true;
}

void UMeshComponent::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);
}

void UMeshComponent::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);
}
