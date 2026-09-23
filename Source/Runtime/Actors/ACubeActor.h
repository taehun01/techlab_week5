#pragma once

#include "AActor.h"
#include "Runtime/CoreUObject/UCubeComp.h"

// 큐브 액터 정의
class ACubeActor : public AActor
{
	DECLARE_UCLASS(ACubeActor, AActor)
	GENERATED_BODY()

public:
	explicit ACubeActor();

	// 색상 제어
	void SetColor(const FVector& InColor);
	FVector GetColor() const;

	UCubeComp* GetCubeComponent() const;
};
