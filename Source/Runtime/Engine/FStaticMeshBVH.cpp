#include "FStaticMeshBVH.h"

#include "Runtime/Math/FMatrix.h"
#include "Runtime/Rendering/FMesh.h"

void FStaticMeshBVH::Build(const FStaticMesh& Mesh, const FMeshBVHSettings& Settings)
{
	Tree.Build(Mesh.GetPositions(), Mesh.GetIndices(), Settings);
}

void FStaticMeshBVH::Clear()
{
	Tree.Clear();
}

bool FStaticMeshBVH::Raycast(const FRay& WorldRay, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint) const
{
	if (!Tree.IsBuilt())
	{
		return false;
	}

	// Ray를 Object 좌표계로 변환
	FMatrix InvM;
	if (!ModelMatrix.Inverse(InvM))
	{
		return false;
	}

	const FVector LocalOrigin = InvM.TransformPointRow(WorldRay.Origin);
	const FVector LocalDirection = InvM.TransformPointRow(WorldRay.Direction, 0.0f); // w = 0으로 방향 벡터 유지
	const FRay LocalRay{ LocalOrigin, LocalDirection };

	FMeshBVHRayHit Hit;
	if (!Tree.QueryRayClosest(LocalRay, Hit))
	{
		return false;
	}

	// 방향을 정규화하지 않고 변환했으므로 로컬 광선의 t가 곧 월드 광선의 t다.
	OutDistance = Hit.T;
	OutImpactPoint = WorldRay.Origin + WorldRay.Direction * Hit.T;
	return true;
}


bool FStaticMeshBVH::RaycastInverse(const FRay& WorldRay, const FMatrix& InvModelMatrix, float MaxT, float& OutDistance) const
{
	if (!Tree.IsBuilt())
	{
		return false;
	}

	// 방향을 정규화하지 않으므로 로컬 광선의 t가 곧 월드 광선의 t다.
	const FRay LocalRay{
		InvModelMatrix.TransformPointRow(WorldRay.Origin),
		InvModelMatrix.TransformPointRow(WorldRay.Direction, 0.0f)
	};

	FMeshBVHRayHit Hit;
	if (!Tree.QueryRayClosest(LocalRay, Hit, MaxT))
	{
		return false;
	}

	OutDistance = Hit.T;
	return true;
}
