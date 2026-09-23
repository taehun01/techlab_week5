#include "UMeshComponent.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "UClass.h"
#include "Runtime/Engine/UScene.h"

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
}

void UMeshComponent::Unregister()
{
    if (Scene)
    {
        Scene->RemoveRenderComponent(this);
    }
    Super::Unregister();
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
