#pragma once

#include "Runtime/Math/FVector.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Geometry/FRay.h"


class UMeshComponent;
class FStaticMesh;
class FSceneBVH;
struct FAxisAlignedBoundingBox;

namespace FRayCastingManager
{
    FRay CreateRayFromScreenPosition(const FCamera& Camera, const FVector2& MousePosition, const FVector2& ViewportSize);
    bool RayIntersectsMeshes(const FRay& Ray, const FCamera& Camera, const TArray<UMeshComponent*>& Components, UMeshComponent*& HitComponent, FVector& OutImpactPoint);

    // 씬 BVH(TLAS) + 메시 BVH(BLAS)로 가장 가까운 메시를 찾는다.
    // 카메라를 향하는 컴포넌트는 BVH에 없으므로 먼저 따로 검사하고, 그 거리를 BVH 순회의 상한으로 넘긴다.
    bool RaycastSceneBVH(const FRay& Ray, const FCamera& Camera, const FSceneBVH& SceneBVH, UMeshComponent*& HitComponent, FVector& OutImpactPoint);
    bool RayIntersectsAABB(const FRay& Ray, const FAxisAlignedBoundingBox& AABB);
    bool RayIntersectsMesh(const FRay& Ray, const FStaticMesh& Mesh, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint);
    bool RayIntersectsTriangle(const FRay& Ray, const FVector& A, const FVector& B, const FVector& C, float& OutT);
};
