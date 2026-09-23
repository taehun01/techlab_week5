#include "UPrimitiveComponent.h"
#include "Runtime/Actors/AActor.h"
#include "Runtime/Engine/FArchive.h"
#include "Runtime/Engine/UScene.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Rendering/ShaderConstants.h"
#include "UClass.h"
#include <numbers>

IMPLEMENT_UCLASS(UPrimitiveComponent, USceneComponent)

namespace
{
    static const FRenderData GEmptyRenderData = {
        .MeshId = FName("None"),
        .MaterialId = FName("None"),
        .TextureId = FName("None"),
        .bSelected = false,
    };
}

void UPrimitiveComponent::Initialize()
{
    Super::Initialize();
}

void UPrimitiveComponent::Register(UScene& InScene)
{
    Super::Register(InScene);
}

void UPrimitiveComponent::Unregister()
{
    Super::Unregister();
}

void UPrimitiveComponent::SetRelativeTransform(const FTransform& InRelativeTransform)
{
    RelativeTransform = InRelativeTransform;
}

FTransform UPrimitiveComponent::GetGlobalTransform() const
{
    if (SceneOwner)
    {
        if (auto* ParentPrim = SceneOwner->Cast<UPrimitiveComponent>())
        {
            return ParentPrim->GetGlobalTransform() * RelativeTransform;
        }
    }

    if (!ActorOwner || ActorOwner->GetRootComponent() == this)
    {
        return RelativeTransform;
    }

    if (auto* RootPrim = ActorOwner->GetRootComponent()->Cast<UPrimitiveComponent>())
    {
        FTransform ParentWorld = RootPrim->GetGlobalTransform();
        if (!bInheritRotation)
        {
            // 부모 회전 무시하고 위치와 스케일만 상속
            FTransform Result;
            Result.Scale3D = RelativeTransform.Scale3D;
            Result.Rotation = RelativeTransform.Rotation;
            Result.Location = ParentWorld.Location + RelativeTransform.Location;
            return Result;
        }
        return ParentWorld * RelativeTransform;
    }

    return RelativeTransform;
}

FMatrix UPrimitiveComponent::GetModelMatrix()
{
    return GetGlobalTransform().ToMatrix();
}

void UPrimitiveComponent::Serialize(FArchive& Archive) const
{
    Super::Serialize(Archive);

    Archive.SetVector("Location", RelativeTransform.Location);
    Archive.SetVector("Rotation", RelativeTransform.Rotation.GetEulerXYZ());
    Archive.SetVector("Scale", RelativeTransform.Scale3D);
    Archive.SetVector("Color", Color);
}

void UPrimitiveComponent::Deserialize(const FArchive& Archive)
{
    Super::Deserialize(Archive);

    RelativeTransform.Location = Archive.GetVector("Location");

    constexpr float RadToDeg = 180.0f / std::numbers::pi_v<float>;
    FVector Rotation = Archive.GetVector("Rotation");
    for (int i = 0; i < 3; ++i)
    {
        Rotation[i] *= RadToDeg;
    }
    RelativeTransform.Rotation = FQuaternion::FromEulerXYZDeg(Rotation);

    RelativeTransform.Scale3D = Archive.GetVector("Scale");
    Color = Archive.GetVector("Color");
}



FAxisAlignedBoundingBox UPrimitiveComponent::CalcLocalBounds()
{
    return {};
}
