#include "FPrimitiveVisualizer.h"

#include "Runtime/CoreUObject/UPrimitiveComponent.h"
#include "Runtime/Geometry/FAxisAlignedBoundingBox.h"
#include "Runtime/Engine/FRenderView.h"
#include "Runtime/Rendering/FMesh.h"
#include "Runtime/Rendering/FRenderResourceLibrary.h"
#include "Runtime/Math/FMatrix.h"
#include <Runtime\CoreUObject\UMeshComponent.h>

void FPrimitiveVisualizer::Draw(
	const UMeshComponent& Component,
	FRenderView& RenderView,
	const FCamera& Camera,
	const FVector4& Color
) const
{
    if (Component.IsA<UMeshComponent>() == false) { return; }

    auto Mesh = FRenderResourceLibrary::Get().GetMesh(Component.GetPureRenderData().MeshId);
    if (!Mesh) return;
    const FMatrix ModelMatrix = Component.GetRenderMatrix(Camera);
    FAxisAlignedBoundingBox AABB{ *Mesh, ModelMatrix };
    RenderView.RenderBoxMinMax(AABB.Min, AABB.Max, Color);
}
