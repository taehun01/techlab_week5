#pragma once

#include "UStaticMeshComponent.h"
#include "Runtime/Engine/UScene.h"

class UCylinderComp : public UStaticMeshComponent
{
	DECLARE_UCLASS(UCylinderComp, UStaticMeshComponent)
	GENERATED_BODY()

protected:
	explicit UCylinderComp() = default;

public:
	void Initialize() override;
};
