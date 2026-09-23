#include "AStaticMeshActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"

IMPLEMENT_UCLASS(AStaticMeshActor, AActor)
UCLASS_META(AStaticMeshActor, DisplayName, "Static Mesh Actor")

AStaticMeshActor::AStaticMeshActor()
{
	// 정적 메시 컴포넌트 생성 및 루트 장착
	CreateRootComponent(UStaticMeshComponent::StaticClass());
	SetColor(FVector{ 1.0f, 1.0f, 1.0f });

	if (auto* Comp = GetStaticMeshComponent())
	{
		Comp->SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("Cube"));
		//Comp->SetMaterial(0, FName("Simple"));
	}
}

UStaticMeshComponent* AStaticMeshActor::GetStaticMeshComponent() const
{
	return RootComponent ? RootComponent->Cast<UStaticMeshComponent>() : nullptr;
}

void AStaticMeshActor::SetStaticMesh(UStaticMesh* InStaticMesh)
{
	if (auto* Comp = GetStaticMeshComponent())
	{
		Comp->SetStaticMesh(InStaticMesh);
	}
}

void AStaticMeshActor::SetStaticMesh(const FName& InMeshId)
{
	if (auto* Comp = GetStaticMeshComponent())
	{
		if (UStaticMesh* Shared = FRenderResourceLibrary::Get().GetUStaticMesh(InMeshId))
		{
			Comp->SetStaticMesh(Shared);
		}
	}
}
