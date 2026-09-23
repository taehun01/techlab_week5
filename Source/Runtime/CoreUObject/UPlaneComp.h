#pragma once

#include "UStaticMeshComponent.h"

class UPlaneComp : public UStaticMeshComponent
{
	DECLARE_UCLASS(UPlaneComp, UStaticMeshComponent)
	GENERATED_BODY()

protected:
	explicit UPlaneComp() = default;

public:
	void Initialize() override;
};
