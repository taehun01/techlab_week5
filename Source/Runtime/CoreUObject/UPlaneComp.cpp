#include "UPlaneComp.h"

#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "Runtime/Engine/UScene.h"
#include "UClass.h"


IMPLEMENT_UCLASS(UPlaneComp, UStaticMeshComponent)
UCLASS_META(UPlaneComp, DisplayName, "Plane")
UCLASS_META(UPlaneComp, MeshName, "Plane")

void UPlaneComp::Initialize() {
  Super::Initialize();
  SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("Plane"));
  SetMaterial(0, FName("Simple"));
}
