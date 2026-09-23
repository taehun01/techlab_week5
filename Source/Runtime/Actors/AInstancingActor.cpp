#include "AInstancingActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UInstancePrimitiveComponent.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include <Runtime\Core\FName.h>


IMPLEMENT_UCLASS(AInstancingActor, AActor)
UCLASS_META(AInstancingActor, DisplayName, "Instancing Actor")

AInstancingActor::AInstancingActor()
{
	CreateRootComponent(UInstancePrimitiveComponent::StaticClass());

	if (auto* PrimComp = GetRootComponent()->Cast<UInstancePrimitiveComponent>())
	{

		PrimComp->SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("MasterYi"));
		PrimComp->SetMaterial(0, FName("Instance_Textured"));
		PrimComp->SetTextureID(FName("MasterYi_Head"));
	}

	FTransform DefaultTransform;
	DefaultTransform.Scale3D = FVector(5.0f, 5.0f, 5.0f);
	SetTransform(DefaultTransform);

}
