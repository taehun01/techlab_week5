#pragma once

#include "Runtime/Geometry/FMeshBVH.h"

class FStaticMesh;
struct FMatrix;

// FStaticMesh와 FMeshBVH를 연결한다.
// BVH는 메시 로컬 공간 기준으로 한 번 빌드하고, 인스턴스별 트랜스폼은 쿼리 시 광선을 로컬로 옮겨 처리한다.
class FStaticMeshBVH
{
public:
	void Build(const FStaticMesh& Mesh, const FMeshBVHSettings& Settings = {});
	void Clear();

	bool IsBuilt() const { return Tree.IsBuilt(); }
	const FMeshBVH& GetTree() const { return Tree; }

	// 월드 광선으로 가장 가까운 삼각형을 찾는다. OutDistance는 월드 광선 기준 거리.
	bool Raycast(const FRay& WorldRay, const FMatrix& ModelMatrix, float& OutDistance, FVector& OutImpactPoint) const;

private:
	FMeshBVH Tree;
};
