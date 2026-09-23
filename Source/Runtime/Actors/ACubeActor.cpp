#include "ACubeActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"

IMPLEMENT_UCLASS(ACubeActor, AActor)
UCLASS_META(ACubeActor, DisplayName, "Cube Actor")

ACubeActor::ACubeActor()
{
	// 기본 큐브 컴포넌트 장착
	CreateRootComponent(UCubeComp::StaticClass());
	SetColor(FVector{ 1.0f, 1.0f, 1.0f });
}

void ACubeActor::SetColor(const FVector& InColor)
{
	if (auto* Comp = GetCubeComponent())
	{
		Comp->SetColor(InColor);
	}
}

FVector ACubeActor::GetColor() const
{
	if (auto* Comp = GetCubeComponent())
	{
		return Comp->GetColor();
	}
	return FVector{ 1.0f, 1.0f, 1.0f };
}

UCubeComp* ACubeActor::GetCubeComponent() const
{
	return RootComponent ? RootComponent->Cast<UCubeComp>() : nullptr;
}
