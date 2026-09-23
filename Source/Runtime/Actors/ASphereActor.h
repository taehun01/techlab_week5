#pragma once

#include "AActor.h"
#include "Runtime/CoreUObject/USphereComp.h"

// 구체 액터 정의
class ASphereActor : public AActor
{
	DECLARE_UCLASS(ASphereActor, AActor)
	GENERATED_BODY()

public:
	explicit ASphereActor();

	// 색상 제어
	void SetColor(const FVector& InColor);
	FVector GetColor() const;

	USphereComp* GetSphereComponent() const;
};
