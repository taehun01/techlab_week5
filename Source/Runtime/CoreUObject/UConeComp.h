#pragma once

#include "UStaticMeshComponent.h"
#include "Runtime/Engine/UScene.h"

class UConeComp : public UStaticMeshComponent
{
	DECLARE_UCLASS(UConeComp, UStaticMeshComponent)
	GENERATED_BODY()

protected:
	explicit UConeComp() = default;

public:
	void Initialize() override;
};
