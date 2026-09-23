#include "UConeComp.h"
#include "Runtime/CoreUObject/UObjectGlobals.h"
#include "Runtime/CoreUObject/UStaticMesh.h"
#include "UClass.h"

IMPLEMENT_UCLASS(UConeComp, UStaticMeshComponent)
UCLASS_META(UConeComp, DisplayName, "Cone")
UCLASS_META(UConeComp, MeshName, "Cone")

void UConeComp::Initialize() {
  Super::Initialize();
  SetStaticMesh(FRenderResourceLibrary::Get().GetUStaticMesh("Cone"));
  SetMaterial(0, FName("Simple"));
}
