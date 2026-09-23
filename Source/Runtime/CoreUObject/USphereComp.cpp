#include "USphereComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "UClass.h"

IMPLEMENT_UCLASS(USphereComp, UStaticMeshComponent)
UCLASS_META(USphereComp, DisplayName, "Sphere")
UCLASS_META(USphereComp, MeshName, "Sphere")

void USphereComp::Initialize() {
  Super::Initialize();
  SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("Sphere"));
  SetMaterial(0, FName("Simple"));
}
