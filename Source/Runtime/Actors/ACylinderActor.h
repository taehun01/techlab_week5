#pragma once

#include "AActor.h"
#include "Runtime/CoreUObject/UCylinderComp.h"

// 실린더 액터 정의
class ACylinderActor : public AActor
{
	DECLARE_UCLASS(ACylinderActor, AActor)
	GENERATED_BODY()

public:
	explicit ACylinderActor();

	// 색상 제어
	void SetColor(const FVector& InColor);
	FVector GetColor() const;

	UCylinderComp* GetCylinderComponent() const;
};
