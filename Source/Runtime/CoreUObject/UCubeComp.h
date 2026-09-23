#pragma once

#include "UStaticMeshComponent.h"
#include "Runtime/Engine/UScene.h"

class UCubeComp : public UStaticMeshComponent
{
	DECLARE_UCLASS(UCubeComp, UStaticMeshComponent)
	GENERATED_BODY()

protected:
	explicit UCubeComp() = default;

public:
	void Initialize() override;
};
