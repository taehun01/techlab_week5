#pragma once

#include "Runtime/Math/FVector.h"
#include "Runtime/Core/TArray.h"
#include "Runtime/Math/FVector2.h"
#include "Runtime/Engine/FCamera.h"
#include "Runtime/Geometry/FRay.h"


class UMeshComponent;
class FStaticMesh;
class FSceneOctree;
struct FAxisAlignedBoundingBox;

namespace FRayCastingManager
{
    FRay CreateRayFromScreenPosition(const FCamera& Camera, const FVector2& MousePosition, const FVector2& ViewportSize);
    bool RayIntersectsMeshes(const FRay& Ray, const FCamera& Camera, const TArray<UMeshComponent*>& Components, UMeshComponent*& HitComponent, FVector& OutImpactPoint);

    // 옥트리로 후보를 좁혀 가장 가까운 메시를 찾는다.
    // 후보를 AABB 진입 거리 순으로 검사하다가, 진입 거리가 이미 찾은 충돌보다 멀면 중단한다.
    // 카메라를 향하는 컴포넌트는 옥트리에 없으므로 따로 전부 검사한다.
    bool RaycastScene(const FRay& Ray, const FCamera& Camera, const FSceneOctree& Octree, UMeshComponent*& HitComponent, FVector& OutImpactPoint);
    bool RayIntersectsAABB(const FRay& Ray, const FAxisAlignedBoundingBox& AABB);
    bool RayIntersectsMesh(const FRay& Ray, const FStaticMesh& Mesh, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint);
    bool RayIntersectsTriangle(const FRay& Ray, const FVector& A, const FVector& B, const FVector& C, float& OutT);
};
