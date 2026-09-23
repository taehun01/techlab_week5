#include "UClass.h"
#include "USceneComponent.h"
#include "ThirdParty/Json/nlohmann/json.hpp"
#include "UObjectGlobals.h" 
#include "UPrimitiveComponent.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Engine/UScene.h"


IMPLEMENT_UCLASS(USceneComponent, UObject)

void USceneComponent::Initialize()
{
    Super::Initialize();
    Scene = nullptr;
    bHasBegunPlay = false;
}
void USceneComponent::Release()
{
    if (bHasBegunPlay) { EndPlay(); }
    if (Scene) { Unregister(); }

    ActorOwner = nullptr;
    SceneOwner = nullptr;
    Scene = nullptr;

    Super::Release();
}

void USceneComponent::Register(UScene& InScene)
{
    if (Scene == &InScene) { return; }
    if (Scene) { Unregister(); }

    Scene = &InScene;
}

void USceneComponent::BeginPlay()
{
    if (!Scene || bHasBegunPlay) { return; }
    bHasBegunPlay = true;
}

void USceneComponent::EndPlay()
{
    if (!bHasBegunPlay) { return; }
    bHasBegunPlay = false;
}

void USceneComponent::Unregister()
{
    if (bHasBegunPlay) { EndPlay(); }
    Scene = nullptr;
}

void USceneComponent::SetupAttachment(USceneComponent* InParent)
{
    if (InParent == this) { return; }

    SceneOwner = InParent;
    if (InParent)
    {
        ActorOwner = InParent->GetActorOwner();
    }
}

void USceneComponent::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);
}

void USceneComponent::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);
}

