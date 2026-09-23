#include "ABillboardActor.h"
#include "Runtime/CoreUObject/UClass.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UBillboardComp.h"

IMPLEMENT_UCLASS(ABillboardActor, AActor)
UCLASS_META(ABillboardActor, DisplayName, "Billboard Actor")

ABillboardActor::ABillboardActor()
{
	// 기본 큐브 컴포넌트 장착
	CreateRootComponent(UBillBoardComp::StaticClass());
	
	if (auto* PrimComp = GetRootComponent()->Cast<UBillBoardComp>())
	{
		PrimComp->SetTextureID(FString("masteryi"));
	}
}

UBillBoardComp* ABillboardActor::GetBillboardComponent() const
{
	return RootComponent ? RootComponent->Cast<UBillBoardComp>() : nullptr;
}
