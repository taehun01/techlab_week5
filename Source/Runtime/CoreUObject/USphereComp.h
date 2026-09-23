#pragma once

#include "UStaticMeshComponent.h"
#include "Runtime/Engine/UScene.h"

class USphereComp : public UStaticMeshComponent
{
	DECLARE_UCLASS(USphereComp, UStaticMeshComponent)
	GENERATED_BODY()

protected:
	explicit USphereComp() = default;

public:
	void Initialize() override;
};
