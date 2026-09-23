#include "ACylinderActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"

IMPLEMENT_UCLASS(ACylinderActor, AActor)
UCLASS_META(ACylinderActor, DisplayName, "Cylinder Actor")

ACylinderActor::ACylinderActor()
{
	// 기본 실린더 컴포넌트 장착
	CreateRootComponent(UCylinderComp::StaticClass());
	SetColor(FVector{ 1.0f, 1.0f, 1.0f });
}

void ACylinderActor::SetColor(const FVector& InColor)
{
	if (auto* Comp = GetCylinderComponent())
	{
		Comp->SetColor(InColor);
	}
}

FVector ACylinderActor::GetColor() const
{
	if (auto* Comp = GetCylinderComponent())
	{
		return Comp->GetColor();
	}
	return FVector{ 1.0f, 1.0f, 1.0f };
}

UCylinderComp* ACylinderActor::GetCylinderComponent() const
{
	return RootComponent ? RootComponent->Cast<UCylinderComp>() : nullptr;
}
