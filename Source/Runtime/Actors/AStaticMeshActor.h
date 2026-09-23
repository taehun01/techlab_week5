#pragma once

#include "AActor.h"
#include "Runtime/CoreUObject/UStaticMeshComponent.h"

// 정적 메시 액터 정의
class AStaticMeshActor : public AActor
{
	DECLARE_UCLASS(AStaticMeshActor, AActor)
	GENERATED_BODY()

public:
	explicit AStaticMeshActor();

	UStaticMeshComponent* GetStaticMeshComponent() const;
	void SetStaticMesh(UStaticMesh* InStaticMesh);
	void SetStaticMesh(const FName& InMeshId);
};
