#include "ASphereActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"

IMPLEMENT_UCLASS(ASphereActor, AActor)
UCLASS_META(ASphereActor, DisplayName, "Sphere Actor")

ASphereActor::ASphereActor()
{
	// 기본 구체 컴포넌트 장착
	CreateRootComponent(USphereComp::StaticClass());
	SetColor(FVector{ 1.0f, 1.0f, 1.0f });
}

void ASphereActor::SetColor(const FVector& InColor)
{
	if (auto* Comp = GetSphereComponent())
	{
		Comp->SetColor(InColor);
	}
}

FVector ASphereActor::GetColor() const
{
	if (auto* Comp = GetSphereComponent())
	{
		return Comp->GetColor();
	}
	return FVector{ 1.0f, 1.0f, 1.0f };
}

USphereComp* ASphereActor::GetSphereComponent() const
{
	return RootComponent ? RootComponent->Cast<USphereComp>() : nullptr;
}
