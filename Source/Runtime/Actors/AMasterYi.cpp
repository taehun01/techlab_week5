#include "AMasterYi.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"

IMPLEMENT_UCLASS(AMasterYi, AActor)
UCLASS_META(AMasterYi, DisplayName, "MasterYi Actor")

AMasterYi::AMasterYi()
{
	// 정적 메시 컴포넌트 생성 및 루트 장착
	CreateRootComponent(UStaticMeshComponent::StaticClass());

	if (auto* MeshComp = GetStaticMeshComponent())
	{

		MeshComp->SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("MasterYi"));
		MeshComp->SetTextureID(FName("MasterYi_Head"));
	}

	FTransform DefaultTransform;
	DefaultTransform.Scale3D = FVector(5.0f, 5.0f, 5.0f);
	SetTransform(DefaultTransform);
}

UStaticMeshComponent* AMasterYi::GetStaticMeshComponent() const
{
	return RootComponent ? RootComponent->Cast<UStaticMeshComponent>() : nullptr;
}
